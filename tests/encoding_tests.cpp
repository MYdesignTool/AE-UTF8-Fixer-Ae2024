#include "Encoding.h"
#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>
#define CHECK(expr) do { ++checks; if (!(expr)) { std::printf("FAIL line %d: %s\n", __LINE__, #expr); return 1; } } while (0)
int main() {
    int checks{};
    using namespace bridge;
    CHECK(IsSupported({24,0,0}) && !IsSupported({23,9,0}) && !IsSupported({}));
    for (auto language : {"en", "en_US", "en-GB", "EN_US", "zh", "zh_CN", "zh_TW", "zh-Hans", "zh-Hant", "ZH_CN"})
        CHECK(IsSupportedLanguage(language));
    for (auto language : {"", "e", "z", "english", "zhcn", "en_", "zh-", "ja_JP", "de_DE", "fr_FR", "unknown"})
        CHECK(!IsSupportedLanguage(language));
    CHECK(IsSupportedLanguage("en_US", true));
    CHECK(!IsSupportedLanguage("zh_CN", true));
    CHECK(!IsSupportedLanguage("zh_TW", true));
    CHECK(!UseLegacyBridge({23,6,0}, BridgeMode::Auto));
    CHECK(!UseLegacyBridge({23,6,0}, BridgeMode::On));
    CHECK(!UseLegacyBridge({}, BridgeMode::On));
    CHECK(UseLegacyBridge({24,6,0}, BridgeMode::Auto));
    CHECK(UseLegacyBridge({24,0,0}, BridgeMode::Auto));
    CHECK(UseLegacyBridge({25,0,0}, BridgeMode::Auto));
    CHECK(UseLegacyBridge({26,0,0}, BridgeMode::Auto));
    CHECK(UseLegacyBridge({26,1,0}, BridgeMode::Auto));
    CHECK(!UseLegacyBridge({26,2,0}, BridgeMode::Auto));
    CHECK(!UseLegacyBridge({27,0,0}, BridgeMode::Auto));
    CHECK(UseLegacyBridge({27,0,0}, BridgeMode::On));
    CHECK(!UseLegacyBridge({24,0,0}, BridgeMode::Off));
    CHECK(IsValidUTF8("") && IsValidUTF8("English"));
    CHECK(IsValidUTF8("\xC3\xA9") && IsValidUTF8("\xF0\x9F\x98\x80"));
    for (auto s : {"\xC0\xAF", "\xED\xA0\x80", "\xF4\x90\x80\x80", "\xE4\xB8", "\x80", "\xF8\x88\x80\x80\x80"})
        CHECK(!IsValidUTF8(s));
    const std::string gbk = "\xD6\xD0\xCE\xC4";
    const std::string utf8 = "\xE4\xB8\xAD\xE6\x96\x87";
    CHECK(Normalize(gbk, SourceEncoding::Auto, &MultiByteToWideChar) == utf8);
    CHECK(Normalize(utf8, SourceEncoding::Auto, &MultiByteToWideChar).empty());
    CHECK(Normalize("English", SourceEncoding::Auto, &MultiByteToWideChar).empty());
    CHECK(Normalize("\xE9", SourceEncoding::Auto, &MultiByteToWideChar).empty());
    CHECK(Normalize("\xFF\xFF", SourceEncoding::Auto, &MultiByteToWideChar).empty());
    CHECK(Normalize(gbk, SourceEncoding::UTF8, &MultiByteToWideChar).empty());
    for (size_t n = 0; n <= utf8.size(); ++n) CHECK(IsValidUTF8(std::string_view(utf8).substr(0, UTF8Prefix(utf8, n))));
    CHECK(UTF8Prefix(utf8, 4) == 3);
    RegisterDisplayText(utf8 + "|Off|" + utf8 + "2");
    CHECK(IsRegisteredDisplayText(utf8));
    CHECK(!IsRegisteredDisplayText("Unregistered"));
    wchar_t output[20]{};
    CHECK(DecodeDisplayText(&MultiByteToWideChar, 1252, 0, utf8.c_str(), -1, nullptr, 0, true) == 3);
    CHECK(DecodeDisplayText(&MultiByteToWideChar, 1252, MB_PRECOMPOSED, utf8.c_str(), -1, output, 20, true) == 3);
    CHECK(std::wstring(output) == L"\x4E2D\x6587");
    CHECK(DecodeDisplayText(&MultiByteToWideChar, 1252, 0, utf8.data(), static_cast<int>(utf8.size()), output, 20, true) == 2);
    SetLastError(0);
    CHECK(DecodeDisplayText(&MultiByteToWideChar, 1252, 0, utf8.c_str(), -1, output, 1, true) == 0);
    CHECK(GetLastError() == ERROR_INSUFFICIENT_BUFFER);
    CHECK(DecodeDisplayText(&MultiByteToWideChar, 1252, 0, utf8.c_str(), -1, output, 20, false) == 7);
    CHECK(DecodeDisplayText(&MultiByteToWideChar, 1252, 0, "English", -1, output, 20, true) == 8);
    CHECK(DecodeDisplayText(&MultiByteToWideChar, CP_UTF8, 0, utf8.c_str(), -1, output, 20, true) == 3);
    CHECK(DecodeDisplayText(&MultiByteToWideChar, 1252, 0, utf8.data(), 0, output, 20, true) == 0);
    CHECK(DecodeDisplayText(&MultiByteToWideChar, 1252, 0, utf8.data(), -2, output, 20, true) ==
          MultiByteToWideChar(1252, 0, utf8.data(), -2, output, 20));
    auto stable = Intern(utf8);
    std::atomic<bool> passed{true};
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) threads.emplace_back([&] {
        for (int i = 0; i < 1000; ++i) {
            auto text = utf8 + std::to_string(i);
            RegisterDisplayText(text); Intern(text);
            if (!IsRegisteredDisplayText(text) || std::string(stable) != utf8) passed = false;
        }
    });
    for (auto& thread : threads) thread.join();
    CHECK(passed.load() && stable == Intern(utf8));
    std::printf("PASS: %d encoding checks, 8 concurrent registry/cache workers.\n", checks);
}
