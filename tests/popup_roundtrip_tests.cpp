#include "Encoding.h"
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>
#include <atomic>
#define CHECK(expr) do { ++checks; if (!(expr)) { std::printf("FAIL line %d: %s\n", __LINE__, #expr); return 1; } } while (0)
static int WINAPI UnicodeForwardSpy(UINT cp, DWORD, LPCWCH, int, LPSTR, int, LPCCH fallback, LPBOOL) {
    return cp == CP_UTF8 && fallback && *fallback == '?' ? 777 : 888;
}
int main() {
    using namespace bridge;
    int checks{};
    // Same 14-item topology as P_Fill's embedded English menu, with translated
    // test labels. These are fixtures, not extracted Chinese from the sample AEX.
    const std::string list = "正常|(-|相加|相减|正片叠底|滤色|叠加|强光|柔光|(-|变暗|变亮|差值|排除";
    RegisterDisplayText(list);
    std::vector<std::string> items;
    for (size_t begin = 0;;) {
        auto end = list.find('|', begin);
        items.emplace_back(list.substr(begin, end == list.npos ? list.size()-begin : end-begin));
        if (end == list.npos) break;
        begin = end+1;
    }
    CHECK(items.size() == 14 && items[1] == "(-" && items[9] == "(-");
    for (const auto& item : items) {
        wchar_t wide[50]{};
        const auto chars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, item.c_str(), -1, wide, 50);
        CHECK(chars > 0);
        // Show the exact information-losing step without the new encoder bridge.
        char lossy[100]{};
        BOOL original_used = FALSE;
        CHECK(WideCharToMultiByte(1252, WC_NO_BEST_FIT_CHARS, wide, -1, lossy, 100, nullptr, &original_used) > 0);
        if (item != "(-") CHECK(original_used && std::strchr(lossy, '?'));
        auto n = EncodeDisplayText(&WideCharToMultiByte, 1252, 0, wide, -1, nullptr, 0, nullptr, nullptr, true);
        CHECK(n == item.size() + 1);
        std::string encoded(n, '\0');
        BOOL used = TRUE;
        CHECK(EncodeDisplayText(&WideCharToMultiByte, 1252, 0, wide, -1, encoded.data(), n, "?", &used, true) == n);
        CHECK(!used && encoded.c_str() == item);
        wchar_t displayed[50]{};
        CHECK(DecodeDisplayText(&MultiByteToWideChar, 1252, 0, encoded.c_str(), -1, displayed, 50, true) == chars);
        CHECK(std::wstring(displayed) == wide);
    }
    const auto word = L"正片叠底";
    CHECK(IsRegisteredWideDisplayText(word));
    CHECK(!IsRegisteredWideDisplayText(L"未登记"));
    char output[100]{};
    CHECK(EncodeDisplayText(&WideCharToMultiByte, 1252, 0, word, 4, output, 100, nullptr, nullptr, true) == 12);
    CHECK(std::string_view(output, 12) == "正片叠底");
    CHECK(EncodeDisplayText(&WideCharToMultiByte, 1252, 0, word, 5, output, 100, nullptr, nullptr, true) == 13);
    CHECK(!std::strcmp(output, "正片叠底"));
    BOOL used = TRUE;
    SetLastError(1234);
    CHECK(EncodeDisplayText(&WideCharToMultiByte, 1252, WC_NO_BEST_FIT_CHARS, word, -1, output, 100, "!", &used, true) == 13);
    CHECK(!used && GetLastError() == 1234);
    SetLastError(0);
    CHECK(EncodeDisplayText(&WideCharToMultiByte, 1252, 0, word, -1, output, 5, nullptr, nullptr, true) == 0);
    CHECK(GetLastError() == ERROR_INSUFFICIENT_BUFFER && EncoderBufferFailureCount() > 0);
    CHECK(EncodeDisplayText(&WideCharToMultiByte, 1252, 0, word, -1, output, 100, nullptr, &used, false) == 5);
    CHECK(used && !std::strcmp(output, "????"));
    CHECK(EncodeDisplayText(&WideCharToMultiByte, 1252, 0, L"未登记", -1, output, 100, nullptr, nullptr, true) == 4);
    CHECK(!std::strcmp(output, "???"));
    CHECK(EncodeDisplayText(&WideCharToMultiByte, 1252, 0, L"English", -1, output, 100, nullptr, nullptr, true) == 8);
    CHECK(!std::strcmp(output, "English"));
    // A lossless requested encoding must not be changed to UTF-8.
    CHECK(EncodeDisplayText(&WideCharToMultiByte, 936, 0, word, -1, output, 100, nullptr, nullptr, true) == 9);
    CHECK(EncodeDisplayText(&WideCharToMultiByte, CP_UTF8, 0, word, -1, output, 100, nullptr, nullptr, true) == 13);
    CHECK(EncodeDisplayText(&WideCharToMultiByte, 1252, 0, word, 0, output, 100, nullptr, nullptr, true) == 0);
    CHECK(EncodeDisplayText(&WideCharToMultiByte, 1252, 0, nullptr, -1, output, 100, nullptr, nullptr, true) == 0);
    SetLastError(0);
    CHECK(EncodeDisplayText(&WideCharToMultiByte, 1252, 0x80000000, word, -1, output, 100, nullptr, nullptr, true) == 0);
    CHECK(GetLastError() == ERROR_INVALID_FLAGS);
    // Forward explicit Unicode arguments unchanged; Windows builds differ in
    // whether they reject or ignore the documented-forbidden fallback pointer.
    CHECK(EncodeDisplayText(&UnicodeForwardSpy, CP_UTF8, 0, word, -1, output, 100, "?", nullptr, true) == 777);
    std::atomic<bool> passed{true};
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) threads.emplace_back([&] {
        for (int i = 0; i < 1000; ++i) {
            char encoded[50]{};
            if (EncodeDisplayText(&WideCharToMultiByte, 1252, 0, word, -1, encoded, 50, nullptr, nullptr, true) != 13 ||
                std::strcmp(encoded, "正片叠底")) passed = false;
        }
    });
    for (auto& thread : threads) thread.join();
    CHECK(passed.load() && EncoderHitCount() >= 8000);
    std::printf("PASS: %d popup checks, 14 menu entries, ANSI question-mark reproduction, 8 concurrent workers.\n", checks);
}
