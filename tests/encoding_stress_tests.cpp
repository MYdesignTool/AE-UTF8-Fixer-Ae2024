#include "Encoding.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <cstdint>
#define CHECK(expr) do { ++checks; if (!(expr)) { std::printf("FAIL case %d line %d: %s\n", sample, __LINE__, #expr); return 1; } } while (0)
int main() {
    using namespace bridge;
    int checks{}, sample{};
    uint32_t state = 0x143826AC;
    auto next = [&] { state ^= state << 13; state ^= state >> 17; state ^= state << 5; return state; };
    for (; sample < 4096; ++sample) {
        std::string raw;
        auto length = next() % 40;
        for (unsigned i = 0; i < length; ++i) raw += static_cast<char>(next() & 0xFF);
        const auto normalized = Normalize(raw, SourceEncoding::Auto, &MultiByteToWideChar);
        CHECK(normalized.empty() || IsValidUTF8(normalized));
        CHECK(Normalize(normalized, SourceEncoding::Auto, &MultiByteToWideChar).empty());
        const auto menu = NormalizePopup(raw, SourceEncoding::Auto, &MultiByteToWideChar);
        CHECK(NormalizePopup(menu, SourceEncoding::Auto, &MultiByteToWideChar).empty());
        // Unknown strings must retain the native API's return value, output bytes
        // and last-error behavior across code pages and insufficient buffers.
        const auto input = std::string("unregistered:") + raw;
        const UINT cp = sample % 3 == 0 ? CP_UTF8 : sample % 3 == 1 ? 1252 : 936;
        const auto capacity = static_cast<int>(next() % 65);
        std::array<wchar_t,64> expected{}, actual{};
        expected.fill(0xCCCC); actual = expected;
        SetLastError(1234);
        const auto native = MultiByteToWideChar(cp, MB_ERR_INVALID_CHARS, input.data(), static_cast<int>(input.size()), expected.data(), capacity);
        const auto native_error = GetLastError();
        SetLastError(1234);
        const auto bridged = DecodeDisplayText(&MultiByteToWideChar, cp, MB_ERR_INVALID_CHARS, input.data(), static_cast<int>(input.size()), actual.data(), capacity, true);
        CHECK(bridged == native && GetLastError() == native_error && expected == actual);
        std::wstring wide = L"unregistered:";
        length = next() % 16;
        for (unsigned i = 0; i < length; ++i) wide += static_cast<wchar_t>(next() & 0xFFFF);
        std::array<char,128> expected_bytes{}, actual_bytes{};
        expected_bytes.fill('~'); actual_bytes = expected_bytes;
        BOOL used_native = TRUE, used_actual = TRUE;
        const auto byte_capacity = static_cast<int>(next() % 100);
        const auto flags = cp == CP_UTF8 ? WC_ERR_INVALID_CHARS : WC_NO_BEST_FIT_CHARS;
        SetLastError(1234);
        const auto encoded = WideCharToMultiByte(cp, flags, wide.data(), static_cast<int>(wide.size()), expected_bytes.data(),
                                                 byte_capacity, nullptr, cp == CP_UTF8 ? nullptr : &used_native);
        const auto encoded_error = GetLastError();
        SetLastError(1234);
        const auto reencoded = EncodeDisplayText(&WideCharToMultiByte, cp, flags, wide.data(), static_cast<int>(wide.size()), actual_bytes.data(),
                                                  byte_capacity, nullptr, cp == CP_UTF8 ? nullptr : &used_actual, true);
        CHECK(reencoded == encoded && GetLastError() == encoded_error && expected_bytes == actual_bytes && used_native == used_actual);
    }
    std::printf("PASS: %d checks across %d deterministic malformed-input/native-API cases.\n", checks, sample);
}
