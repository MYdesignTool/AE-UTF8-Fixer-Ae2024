#include "Encoding.h"
#include <algorithm>
#include <atomic>
#include <climits>
#include <mutex>
#include <shared_mutex>
#include <unordered_set>

namespace bridge {
static std::atomic<unsigned long long> decoder_hits{};
static std::atomic<unsigned long long> encoder_hits{}, encoder_buffer_failures{};
unsigned long long DecoderHitCount() noexcept { return decoder_hits.load(std::memory_order_relaxed); }
unsigned long long EncoderHitCount() noexcept { return encoder_hits.load(std::memory_order_relaxed); }
unsigned long long EncoderBufferFailureCount() noexcept { return encoder_buffer_failures.load(std::memory_order_relaxed); }
bool IsSupported(HostVersion v) noexcept { return v.major >= 24; }
bool UseLegacyBridge(HostVersion v, BridgeMode mode) noexcept {
    if (mode == BridgeMode::On) return true;
    if (mode == BridgeMode::Off) return false;
    // Compatibility policy based on the upstream 26.2 boundary, not an SDK guarantee.
    return v.major < 26 || (v.major == 26 && v.minor < 2);
}
bool IsValidUTF8(std::string_view s) noexcept {
    size_t i = 0;
    while (i < s.size()) {
        const auto c = static_cast<unsigned char>(s[i++]);
        if (c < 0x80) continue;
        unsigned n{}, value{}, minimum{};
        if (c >= 0xC2 && c <= 0xDF) { n = 1; value = c & 31; minimum = 0x80; }
        else if (c >= 0xE0 && c <= 0xEF) { n = 2; value = c & 15; minimum = 0x800; }
        else if (c >= 0xF0 && c <= 0xF4) { n = 3; value = c & 7; minimum = 0x10000; }
        else return false;
        if (s.size() - i < n) return false;
        while (n--) {
            const auto next = static_cast<unsigned char>(s[i++]);
            if ((next & 0xC0) != 0x80) return false;
            value = (value << 6) | (next & 63);
        }
        if (value < minimum || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) return false;
    }
    return true;
}
bool ContainsHan(std::wstring_view s) noexcept {
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned c = s[i];
        if (c >= 0xD800 && c <= 0xDBFF && i + 1 < s.size() && s[i+1] >= 0xDC00 && s[i+1] <= 0xDFFF)
            c = 0x10000 + ((c - 0xD800) << 10) + (s[++i] - 0xDC00);
        if ((c >= 0x3400 && c <= 0x9FFF) || (c >= 0xF900 && c <= 0xFAFF) ||
            (c >= 0x20000 && c <= 0x323AF)) return true;
    }
    return false;
}
static std::wstring Decode(std::string_view s, UINT cp, DecodeFn decode) {
    if (s.empty() || s.size() > INT_MAX) return {};
    int n = decode(cp, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) return {};
    std::wstring w(n, L'\0');
    if (decode(cp, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), w.data(), n) != n) return {};
    return w;
}
std::string Normalize(std::string_view s, SourceEncoding source, DecodeFn decode, EncodeFn encode) {
    if (s.empty() || s.find('\0') != s.npos || s.size() > 65535) return {};
    if (IsValidUTF8(s)) return {}; // Valid UTF-8 always wins in auto mode; never double encode.
    if (source == SourceEncoding::UTF8) return {};
    auto wide = Decode(s, 936, decode);
    if (wide.empty() || (source == SourceEncoding::Auto && !ContainsHan(wide))) return {};
    BOOL substituted = FALSE;
    std::string roundtrip(s.size(), '\0');
    int back = encode(936, WC_NO_BEST_FIT_CHARS, wide.data(), static_cast<int>(wide.size()),
                                   roundtrip.data(), static_cast<int>(roundtrip.size()), nullptr, &substituted);
    if (substituted || back != s.size() || roundtrip != s) return {};
    int n = encode(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string result(n, '\0');
    if (encode(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(), static_cast<int>(wide.size()), result.data(), n, nullptr, nullptr) != n) return {};
    return result;
}
static size_t PopupUnitLength(std::string_view s, size_t i) noexcept {
    const auto c = static_cast<unsigned char>(s[i]);
    if (c < 0x80) return 1;
    size_t utf8 = c >= 0xC2 && c <= 0xDF ? 2 : c >= 0xE0 && c <= 0xEF ? 3 : c >= 0xF0 && c <= 0xF4 ? 4 : 0;
    if (utf8 && s.size() - i >= utf8 && IsValidUTF8(s.substr(i, utf8))) return utf8;
    if (c >= 0x81 && c <= 0xFE && i + 1 < s.size()) {
        const auto trail = static_cast<unsigned char>(s[i+1]);
        // GBK trail bytes include 0x7C. That byte is not a menu separator here.
        if (trail >= 0x40 && trail <= 0xFE && trail != 0x7F) return 2;
    }
    return 1;
}
template<class F> static void ForEachPopupItem(std::string_view s, F item) {
    size_t begin = 0, i = 0;
    while (i < s.size()) {
        if (s[i] == '|') { item(s.substr(begin, i-begin)); begin = ++i; }
        else i += PopupUnitLength(s, i);
    }
    item(s.substr(begin)); // Keep empty first/last/adjacent items.
}
std::string NormalizePopup(std::string_view s, SourceEncoding source, DecodeFn decode, EncodeFn encode) {
    if (s.empty() || s.size() > 65535 || s.find('\0') != s.npos) return {};
    if (IsValidUTF8(s) || source == SourceEncoding::UTF8) return {};
    bool strong_utf8 = false;
    ForEachPopupItem(s, [&](std::string_view item) {
        // Three/four-byte UTF-8 characters distinguish common translated UTF-8
        // labels from GBK words whose two bytes also happen to be valid UTF-8.
        if (IsValidUTF8(item)) for (unsigned char c : item)
            if (c >= 0xE0 && c <= 0xF4) strong_utf8 = true;
    });
    if (!strong_utf8) {
        auto whole = Normalize(s, source, decode, encode);
        if (!whole.empty()) return whole; // Retain the existing all-GBK behavior.
    }
    std::string result;
    result.reserve(s.size());
    bool first = true, changed = false;
    ForEachPopupItem(s, [&](std::string_view item) {
        if (!first) result += '|';
        first = false;
        auto fixed = Normalize(item, source, decode, encode);
        changed |= !fixed.empty();
        result.append(fixed.empty() ? item : std::string_view(fixed));
    });
    return changed ? result : std::string{};
}
size_t UTF8Prefix(std::string_view s, size_t capacity) noexcept {
    size_t n = (std::min)(s.size(), capacity);
    if (n < s.size()) while (n && (static_cast<unsigned char>(s[n]) & 0xC0) == 0x80) --n;
    return n;
}
struct Hash {
    using is_transparent = void;
    size_t operator()(std::string_view s) const noexcept { return std::hash<std::string_view>{}(s); }
};
struct WideHash {
    using is_transparent = void;
    size_t operator()(std::wstring_view s) const noexcept { return std::hash<std::wstring_view>{}(s); }
};
struct Store {
    std::shared_mutex mutex;
    std::unordered_set<std::string, Hash, std::equal_to<>> strings, display;
    std::unordered_set<std::wstring, WideHash, std::equal_to<>> wide_display;
};
static Store& Strings() { static auto* store = new Store; return *store; }
const char* Intern(std::string_view s) {
    auto& store = Strings();
    std::unique_lock lock(store.mutex);
    return store.strings.emplace(s).first->c_str();
}
static void RegisterOneDisplayText(std::string_view s) {
    // Decode before taking the lock: conversion hooks also consult this store.
    if (!IsValidUTF8(s)) return;
    auto wide = Decode(s, CP_UTF8, &MultiByteToWideChar);
    if (!ContainsHan(wide)) return;
    auto& store = Strings();
    std::unique_lock lock(store.mutex);
    store.display.emplace(s);
    store.wide_display.emplace(wide);
}
void RegisterDisplayText(std::string_view s) {
    RegisterOneDisplayText(s);
    // One undecodable item must not prevent other valid Chinese items registering.
    ForEachPopupItem(s, [](std::string_view token) { RegisterOneDisplayText(token); });
}
bool IsRegisteredDisplayText(std::string_view s) {
    auto& store = Strings();
    std::shared_lock lock(store.mutex);
    return store.display.find(s) != store.display.end();
}
bool IsRegisteredWideDisplayText(std::wstring_view s) {
    auto& store = Strings();
    std::shared_lock lock(store.mutex);
    return store.wide_display.find(s) != store.wide_display.end();
}
static bool IsLegacyCodePage(UINT cp) noexcept {
    // Explicit UTF-8 must remain untouched even on Windows with ACP=65001.
    return cp != CP_UTF8 && (cp == CP_ACP || cp == CP_OEMCP || cp == CP_THREAD_ACP ||
                            cp == 1252 || cp == GetACP() || cp == GetOEMCP());
}
int DecodeDisplayText(DecodeFn original, UINT cp, DWORD flags, LPCCH input, int count,
                      LPWSTR output, int capacity, bool enabled) {
    // Do not interfere with explicit Unicode conversion, arbitrary legacy encodings,
    // caller errors, paths or unregistered UI strings.
    if (enabled && input && (count == -1 || count > 0) && capacity >= 0 &&
        IsLegacyCodePage(cp) &&
        (flags & ~(MB_PRECOMPOSED | MB_COMPOSITE | MB_USEGLYPHCHARS | MB_ERR_INVALID_CHARS)) == 0 &&
        !((flags & MB_PRECOMPOSED) && (flags & MB_COMPOSITE))) {
        size_t n = count == -1 ? strnlen_s(input, 65536) : static_cast<size_t>(count);
        if (n && n <= 65535) {
            if (input[n-1] == '\0') --n;
            if (IsRegisteredDisplayText(std::string_view(input, n))) {
                decoder_hits.fetch_add(1, std::memory_order_relaxed);
                return original(CP_UTF8, flags & MB_ERR_INVALID_CHARS, input, count, output, capacity);
            }
        }
    }
    return original(cp, flags, input, count, output, capacity);
}
int EncodeDisplayText(EncodeFn original, UINT cp, DWORD flags, LPCWCH input, int count,
                      LPSTR output, int capacity, LPCCH default_char, LPBOOL used_default_char,
                      bool enabled) {
    const auto saved_error = GetLastError();
    constexpr DWORD legacy_flags = WC_COMPOSITECHECK | WC_DISCARDNS | WC_SEPCHARS |
                                   WC_DEFAULTCHAR | WC_NO_BEST_FIT_CHARS;
    if (enabled && input && (count == -1 || count > 0) && capacity >= 0 &&
        IsLegacyCodePage(cp) && (flags & ~legacy_flags) == 0) {
        size_t n = count == -1 ? wcsnlen_s(input, 65536) : static_cast<size_t>(count);
        if (n && n <= 65535) {
            if (input[n-1] == L'\0') --n;
            if (IsRegisteredWideDisplayText(std::wstring_view(input, n))) {
                // Probe via the ORIGINAL function, never the hook. GBK on a Chinese
                // system can represent Han without loss and should stay GBK.
                BOOL substituted = FALSE;
                BOOL validation_used = FALSE;
                const auto valid = original(cp, flags, input, count, nullptr, 0, default_char, &validation_used);
                const auto required = valid > 0 ? original(cp, WC_NO_BEST_FIT_CHARS, input, count,
                                                nullptr, 0, nullptr, &substituted) : 0;
                if (required > 0 && substituted) {
                    encoder_hits.fetch_add(1, std::memory_order_relaxed);
                    // UTF-8 forbids default-character parameters. Preserve the public
                    // used-default output explicitly, and never silently truncate.
                    const auto result = original(CP_UTF8, WC_ERR_INVALID_CHARS, input, count,
                                                  output, capacity, nullptr, nullptr);
                    if (result > 0) {
                        if (used_default_char) *used_default_char = FALSE;
                        SetLastError(saved_error);
                    } else if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                        encoder_buffer_failures.fetch_add(1, std::memory_order_relaxed);
                    }
                    return result;
                }
            }
        }
    }
    SetLastError(saved_error);
    return original(cp, flags, input, count, output, capacity, default_char, used_default_char);
}
}
