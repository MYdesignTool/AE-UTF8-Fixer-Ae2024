#pragma once
#include <windows.h>
#include <string>
#include <string_view>

namespace bridge {
using DecodeFn = decltype(&MultiByteToWideChar);
using EncodeFn = decltype(&WideCharToMultiByte);
enum class SourceEncoding { Auto, GBK, UTF8 };
enum class BridgeMode { Auto, On, Off };
struct HostVersion { unsigned major{}, minor{}, patch{}; };
bool IsSupported(HostVersion version) noexcept;
bool UseLegacyBridge(HostVersion version, BridgeMode mode) noexcept;
bool IsValidUTF8(std::string_view text) noexcept;
bool ContainsHan(std::wstring_view text) noexcept;
// Empty means unchanged/unrecognized. Never replaces invalid bytes with '?'.
std::string Normalize(std::string_view text, SourceEncoding source, DecodeFn decode,
                      EncodeFn encode = &WideCharToMultiByte);
size_t UTF8Prefix(std::string_view text, size_t capacity) noexcept;
// Stable process-lifetime storage; AE can retain popup and checkbox pointers.
const char* Intern(std::string_view text);
void RegisterDisplayText(std::string_view text);
bool IsRegisteredDisplayText(std::string_view text);
bool IsRegisteredWideDisplayText(std::wstring_view text);
unsigned long long DecoderHitCount() noexcept;
unsigned long long EncoderHitCount() noexcept;
unsigned long long EncoderBufferFailureCount() noexcept;
// Preserves Win32 sizing, explicit length, NUL and insufficient-buffer semantics.
int DecodeDisplayText(DecodeFn original, UINT code_page, DWORD flags,
                     LPCCH input, int count, LPWSTR output, int capacity,
                     bool enabled);
// Prevents a known Unicode menu item from being replaced by '?' in an ANSI
// round-trip. Lossless legacy conversions and unregistered text stay untouched.
int EncodeDisplayText(EncodeFn original, UINT code_page, DWORD flags,
                     LPCWCH input, int count, LPSTR output, int capacity,
                     LPCCH default_char, LPBOOL used_default_char, bool enabled);
}
