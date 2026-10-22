#include "AEConfig.h"
#include "../SDKCompat.h"
#include "AE_EffectSuites.h"
#include "AE_GeneralPlug.h"
#include "AE_PluginData.h"
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cwchar>
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#ifndef MOCK_MAJOR
#define MOCK_MAJOR 24
#endif
#ifndef MOCK_MINOR
#define MOCK_MINOR 6
#endif
#ifndef MOCK_PATCH
#define MOCK_PATCH 0
#endif
#define CHECK(expr) do { ++checks; if (!(expr)) { std::printf("FAIL line %d: %s, error=%lu\n", __LINE__, #expr, GetLastError()); return 1; } } while (0)
namespace {
const char* language = "en_US";
std::string language_storage;
std::string language_status;
UINT text_code_page = 1252;
unsigned language_calls{}, language_releases{};
constexpr const char* utf8 = "\xE4\xB8\xAD\xE6\x96\x87";
constexpr const char* gbk = "\xD6\xD0\xCE\xC4";
std::atomic<unsigned> adds{}, updates{};
std::atomic<unsigned> alternative_adds{};
unsigned options_calls{};
std::atomic<bool> passed{true};
std::string last_name, last_popup, metadata;
const char* stable_popup{};
bool active{}, legacy{};
PF_Err Add(PF_ProgPtr, PF_ParamIndex, PF_ParamDef* param) {
    ++adds;
    bool ok = param && !std::strcmp(param->PF_DEF_NAME, active ? utf8 : gbk) &&
              param->uu.id == 1234 && param->u.pd.num_choices == 3 && param->u.pd.value == 2;
    if (!ok) passed = false;
    if (std::strcmp(param->u.pd.u.PF_DEF_NAMESPTR, active ? "\xE4\xB8\xAD\xE6\x96\x87|Off|\xE5\x8F\x82\xE6\x95\xB0" : "\xD6\xD0\xCE\xC4|Off|\xB2\xCE\xCA\xFD")) passed = false;
    wchar_t wide[40]{};
    auto n = MultiByteToWideChar(text_code_page, 0, param->PF_DEF_NAME, -1, wide, 40);
    if (active && legacy && (n != 3 || std::wstring(wide) != L"\x4E2D\x6587")) passed = false;
    // Popup managers may convert their Unicode item text back to ANSI before
    // displaying it. The old decoder-only bridge loses Chinese at this step.
    if (active && legacy) {
        const auto bytes = WideCharToMultiByte(text_code_page, 0, wide, -1, nullptr, 0, nullptr, nullptr);
        std::string menu(bytes, '\0');
        if (!bytes || WideCharToMultiByte(text_code_page, 0, wide, -1, menu.data(), bytes, nullptr, nullptr) != bytes)
            passed = false;
        wchar_t displayed[40]{};
        if (MultiByteToWideChar(text_code_page, 0, menu.c_str(), -1, displayed, 40) != n ||
            std::wstring(displayed) != std::wstring(wide)) passed = false;
    }
    return PF_Err_NONE;
}
PF_Err AlternativeAdd(PF_ProgPtr ref, PF_ParamIndex index, PF_ParamDef* param) {
    ++alternative_adds;
    return Add(ref, index, param);
}
PF_Err Update(PF_ProgPtr, PF_ParamIndex, const PF_ParamDef* param) {
    ++updates;
    const auto name = active ? "\xE5\x8F\x82\xE6\x95\xB0" : "\xB2\xCE\xCA\xFD";
    if (!param || std::strcmp(param->PF_DEF_NAME, name) || param->u.fs_d.slider_min != 4 || param->u.fs_d.slider_max != 97)
        passed = false;
    return PF_Err_NONE;
}
PF_Err GetLanguage(A_char* output) {
    ++language_calls;
    if (language_status == "unterminated") std::memset(output, 'z', PF_APP_LANG_TAG_SIZE);
    else strcpy_s(output, PF_APP_LANG_TAG_SIZE, language);
    return language_status == "error" ? PF_Err_INTERNAL_STRUCT_DAMAGED : PF_Err_NONE;
}
A_Err RegisterDeath(AEGP_PluginID, AEGP_DeathHook, AEGP_DeathRefcon) { return A_Err_NONE; }
PF_Err SetOptions(PF_ProgPtr, const A_char* name) {
    ++options_calls;
    if (!name || std::strcmp(name, active ? "\xE5\x8F\x82\xE6\x95\xB0" : "\xB2\xCE\xCA\xFD")) passed = false;
    return PF_Err_NONE;
}
PFAppSuite6 app{};
PF_ParamUtilsSuite3 paramsuite{};
PF_EffectUISuite1 effectsuite{};
AEGP_RegisterSuite5 registration{};
SPErr Acquire(const char* name, int32 version, const void** output) {
    if (!std::strcmp(name, kPFAppSuite) && version == kPFAppSuiteVersion6 && language_status != "missing") *output = &app;
    else if (!std::strcmp(name, kPFParamUtilsSuite) && version == kPFParamUtilsSuiteVersion3) *output = &paramsuite;
    else if (!std::strcmp(name, kAEGPRegisterSuite) && version == kAEGPRegisterSuiteVersion5) *output = &registration;
    else if (!std::strcmp(name, kPFEffectUISuite) && version == kPFEffectUISuiteVersion1) *output = &effectsuite;
    else { *output = nullptr; return kSPBadParameterError; }
    return kSPNoError;
}
SPErr Release(const char* name, int32) {
    if (!std::strcmp(name, kPFAppSuite)) ++language_releases;
    return kSPNoError;
}
A_Err Metadata2(PF_PluginDataPtr, const A_u_char* name, const A_u_char* match, const A_u_char*, const A_u_char* entry,
                A_long, A_long, A_long, A_long, const A_u_char*) {
    metadata = reinterpret_cast<const char*>(name);
    if (std::strcmp(reinterpret_cast<const char*>(match), "TEST.identity.keep") ||
        std::strcmp(reinterpret_cast<const char*>(entry), "CustomChineseEffect")) passed = false;
    return A_Err_NONE;
}
A_Err Metadata1(PF_PluginDataPtr ptr, const A_u_char* name, const A_u_char* match, const A_u_char* category,
                const A_u_char* entry, A_long kind, A_long major, A_long minor, A_long reserved) {
    return Metadata2(ptr, name, match, category, entry, kind, major, minor, reserved, nullptr);
}
}
int wmain(int argc, wchar_t** argv) {
    int checks{};
    CHECK(argc == 8);
    for (const auto c : std::wstring(argv[3])) { CHECK(c < 128); language_storage += static_cast<char>(c); }
    language = language_storage.c_str();
    active = !std::wcscmp(argv[4], L"1");
    legacy = !std::wcscmp(argv[5], L"1");
    text_code_page = static_cast<UINT>(std::wcstoul(argv[6], nullptr, 10));
    for (const auto c : std::wstring(argv[7])) { CHECK(c < 128); language_status += static_cast<char>(c); }
    app.PF_AppGetLanguage = language_status == "null" ? nullptr : &GetLanguage;
    paramsuite.PF_UpdateParamUI = &Update;
    effectsuite.PF_SetOptionsButtonName = &SetOptions;
    registration.AEGP_RegisterDeathHook = &RegisterDeath;
    SPBasicSuite basic{}; basic.AcquireSuite = &Acquire; basic.ReleaseSuite = &Release;
    // Load a PiPL-only effect before the bridge, exercising startup module enumeration.
    auto legacy_module = LoadLibraryW(argv[2]);
    CHECK(legacy_module);
    auto plugin = LoadLibraryW(argv[1]);
    CHECK(plugin);
    using Start = A_Err(*)(SPBasicSuite*, A_long, A_long, AEGP_PluginID, AEGP_GlobalRefcon*);
    auto start = reinterpret_cast<Start>(GetProcAddress(plugin, "EntryPointFunc"));
    CHECK(start && !start(&basic, 999, 999, 1, nullptr)); // SDK ABI numbers are NOT product versions.
    const bool acquired_language = MOCK_MAJOR >= 24 && language_status != "missing";
    CHECK(language_releases == (acquired_language ? 1u : 0u));
    CHECK(language_calls == (acquired_language && language_status != "null" ? 1u : 0u));
    auto oldeffect = reinterpret_cast<PF_Err(*)(PF_Cmd, PF_InData*, PF_OutData*, PF_ParamDef*[], PF_LayerDef*, void*)>(
        GetProcAddress(legacy_module, "OldCustomEffect"));
    CHECK(oldeffect);
    PF_InData in{}; in.inter.add_param = &Add; in.pica_basicP = &basic; in.num_params = 2;
    PF_OutData out{};
    CHECK(!oldeffect(PF_Cmd_PARAMS_SETUP, &in, &out, nullptr, nullptr, nullptr));
    CHECK(in.inter.add_param == &Add && passed.load());
    // Modern metadata-only module has no PiPL and exports a nonstandard effect name.
    wchar_t modern_path[32768]{};
    GetModuleFileNameW(nullptr, modern_path, 32768);
    std::wstring path = modern_path;
    path = path.substr(0, path.find_last_of(L"\\/") + 1) + L"FakeModern.aex";
    // ANSI loading is covered by the GetProcAddress discovery hook.
    auto modern_module = LoadLibraryW(path.c_str());
    CHECK(modern_module);
    auto aegp = reinterpret_cast<Start>(GetProcAddress(modern_module, "EntryPointFunc"));
    CHECK(aegp && aegp(&basic, 10, 20, 1, nullptr) == 30);
    using Effect = PF_Err(*)(PF_Cmd, PF_InData*, PF_OutData*, PF_ParamDef*[], PF_LayerDef*, void*);
    // A cached custom entry can run without metadata registration. Shared host
    // add_param interception must still repair it once discovered from a PiPL effect.
    auto cached = reinterpret_cast<Effect>(GetProcAddress(modern_module, "CustomChineseEffect"));
    CHECK(cached && !cached(PF_Cmd_PARAMS_SETUP, &in, &out, nullptr, nullptr, nullptr));
    CHECK(passed.load());
    auto reg2 = reinterpret_cast<PluginDataEntryFunction2Ptr>(GetProcAddress(modern_module, "PluginDataEntryFunction2"));
    CHECK(reg2 && !reg2(nullptr, &Metadata2, &basic, "After Effects", "24.6.0"));
    CHECK(metadata == (active ? utf8 : gbk) && passed.load());
    auto reg1 = reinterpret_cast<PluginDataEntryFunctionPtr>(GetProcAddress(modern_module, "PluginDataEntryFunction"));
    CHECK(reg1 && !reg1(nullptr, &Metadata1, &basic, "After Effects", "24.6.0"));
    CHECK(metadata == (active ? utf8 : gbk));
    auto modern = reinterpret_cast<Effect>(GetProcAddress(modern_module, "CustomChineseEffect"));
    CHECK(modern && !modern(PF_Cmd_PARAMS_SETUP, &in, &out, nullptr, nullptr, nullptr));
    CHECK(!modern(PF_Cmd_USER_CHANGED_PARAM, &in, &out, nullptr, nullptr, nullptr));
    CHECK(updates == 1 && passed.load());
    auto another = in;
    another.inter.add_param = &AlternativeAdd;
    CHECK(!modern(PF_Cmd_PARAMS_SETUP, &another, &out, nullptr, nullptr, nullptr));
    CHECK(alternative_adds == 1 && another.inter.add_param == &AlternativeAdd);
    std::vector<std::thread> threads;
    for (int i = 0; i < 8; ++i) threads.emplace_back([&] {
        PF_InData input = in; PF_OutData output{};
        for (int j = 0; j < 100; ++j)
            if (modern(PF_Cmd_PARAMS_SETUP, &input, &output, nullptr, nullptr, nullptr)) passed = false;
    });
    for (auto& t : threads) t.join();
    CHECK(adds == 804 && passed.load());
    // A compact host-owned float param is placed just before an inaccessible page.
    SYSTEM_INFO info{}; GetSystemInfo(&info);
    auto memory = static_cast<char*>(VirtualAlloc(nullptr, info.dwPageSize * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    CHECK(memory);
    DWORD old{}; CHECK(VirtualProtect(memory + info.dwPageSize, info.dwPageSize, PAGE_NOACCESS, &old));
    auto compact = reinterpret_cast<PF_ParamDef*>(memory + info.dwPageSize - offsetof(PF_ParamDef,u) - sizeof(PF_FloatSliderDef));
    std::memset(compact, 0, offsetof(PF_ParamDef,u) + sizeof(PF_FloatSliderDef));
    compact->param_type = PF_Param_FLOAT_SLIDER;
    strcpy_s(compact->PF_DEF_NAME, "\xB2\xCE\xCA\xFD");
    compact->u.fs_d.slider_min = 4; compact->u.fs_d.slider_max = 97;
    PF_ParamDef* list[]{nullptr, compact};
    CHECK(!modern(PF_Cmd_UPDATE_PARAMS_UI, &in, &out, list, nullptr, nullptr));
    CHECK(passed.load());
    CHECK(!std::strcmp(compact->PF_DEF_NAME, "\xB2\xCE\xCA\xFD")); // Host-owned params stay intact.
    CHECK(updates == (active ? 2 : 1));
    VirtualFree(memory, 0, MEM_RELEASE);
    CHECK(!modern(PF_Cmd_SEQUENCE_SETUP, &in, &out, nullptr, nullptr, nullptr));
    CHECK(!std::strcmp(out.name, active ? utf8 : gbk) && out.num_params == 88);
    CHECK(!modern(PF_Cmd_SEQUENCE_RESETUP, &in, &out, nullptr, nullptr, nullptr));
    CHECK(!std::strcmp(out.name, active ? utf8 : gbk));
    CHECK(!modern(PF_Cmd_ABOUT, &in, &out, nullptr, nullptr, nullptr));
    CHECK(!std::strcmp(out.return_msg, active ? "\xE5\x8F\x82\xE6\x95\xB0" : "\xB2\xCE\xCA\xFD"));
    CHECK(modern(PF_Cmd_DO_DIALOG, &in, &out, nullptr, nullptr, nullptr) == 321);
    CHECK(options_calls == 1 && passed.load());
    CHECK(!std::strcmp(out.return_msg, active ? "\xE5\x8F\x82\xE6\x95\xB0" : "\xB2\xCE\xCA\xFD"));
    CHECK(out.out_flags & PF_OutFlag_DISPLAY_ERROR_MESSAGE);
    out = {};
    strcpy_s(out.name, gbk);
    strcpy_s(out.return_msg, gbk);
    CHECK(!modern(PF_Cmd_RENDER, &in, &out, nullptr, nullptr, nullptr));
    CHECK(!std::strcmp(out.name, gbk) && !std::strcmp(out.return_msg, gbk) && out.out_flags == 0);
    // Unregistered text is forwarded with its requested code page.
    wchar_t wide[50]{};
    CHECK(MultiByteToWideChar(1252, 0, "English", -1, wide, 50) == 8 && std::wstring(wide) == L"English");
    CHECK(MultiByteToWideChar(1252, 0, "\xC3\xA9", -1, wide, 50) == 3);
    CHECK(MultiByteToWideChar(936, 0, gbk, -1, wide, 50) == 3 && std::wstring(wide) == L"\x4E2D\x6587");
    char lossless[20]{};
    BOOL used_default = TRUE;
    CHECK(WideCharToMultiByte(936, 0, L"\x4E2D\x6587", -1, lossless, 20, nullptr, &used_default) == 5);
    CHECK(!std::strcmp(lossless, gbk) && !used_default);
    std::printf("PASS: %d mock-host checks; AE=%d.%d.%d; UI=%s; text_cp=%u; active=%d; legacy=%d; adds=%u; updates=%u.\n",
        checks, MOCK_MAJOR, MOCK_MINOR, MOCK_PATCH, language, text_code_page, active, legacy, adds.load(), updates.load());
    // Hooks and module pins intentionally live to process exit.
    return 0;
}
