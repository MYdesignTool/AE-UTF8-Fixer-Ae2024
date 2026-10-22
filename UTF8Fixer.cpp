// Derived from AdoreLowen's AE UTF-8 Fixer (MIT). See LICENSE and UPSTREAM.md.
#include "AEConfig.h"
#include "AE_Effect.h"
#include "AE_Macros.h"
#include "AE_EffectSuites.h"
#include "AE_GeneralPlug.h"
#include "AE_PluginData.h"
#include "Encoding.h"
#include "MinHook.h"
#include <tlhelp32.h>
#include <array>
#include <atomic>
#include <cstring>
#include <mutex>
#include <string>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// SDKs before the newer UTF string aliases expose these as plain char fields.
#ifndef PF_DEF_NAME
#define PF_DEF_NAME name
#define PF_DEF_NAMESPTR namesptr
#define PF_DEF_NAMEPTR nameptr
#endif

namespace {
HMODULE self_module{};
bridge::SourceEncoding source_encoding = bridge::SourceEncoding::Auto;
bool legacy_bridge{}, enabled{}, only_english = true;
bridge::DecodeFn original_decode = &MultiByteToWideChar;
bridge::EncodeFn original_encode = &WideCharToMultiByte;
decltype(&LoadLibraryW) original_load{};
decltype(&LoadLibraryExW) original_load_ex{};
decltype(&GetProcAddress) original_get_proc = &GetProcAddress;
std::atomic<unsigned> normalized_count{}, failed_hooks{};
std::atomic<unsigned> popup_placeholder_lists{};
thread_local bool scanning{};
constexpr size_t pool_size = 512;
constexpr size_t effect_pool_size = 2048;
using EffectFn = PF_Err(*)(PF_Cmd, PF_InData*, PF_OutData*, PF_ParamDef*[], PF_LayerDef*, void*);
using AddFn = decltype(PF_InData::inter.add_param);
using UpdateFn = decltype(PF_ParamUtilsSuite3::PF_UpdateParamUI);
std::atomic<UpdateFn> original_update{};
std::mutex hook_mutex;
struct Slot { std::atomic<void*> original{}; HMODULE module{}; };
std::array<Slot, effect_pool_size> effects;
std::array<Slot, pool_size> registration2, registration1;
std::array<Slot, 64> add_callbacks;
size_t effect_count{}, reg2_count{}, reg1_count{}, add_count{};
struct Hook { void* wrapper; void* original; };
std::unordered_map<void*, Hook> hooks;
std::unordered_set<HMODULE> scanned_modules;
std::wstring config_path, log_path;

void Log(const char* text) noexcept {
    OutputDebugStringA(text);
    OutputDebugStringA("\n");
    if (log_path.empty()) return;
    HANDLE file = CreateFileW(log_path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD written{};
    WriteFile(file, text, static_cast<DWORD>(strlen(text)), &written, nullptr);
    WriteFile(file, "\r\n", 2, &written, nullptr);
    CloseHandle(file);
}
bridge::HostVersion ReadHostVersion() {
    wchar_t path[32768]{};
    if (!GetModuleFileNameW(nullptr, path, 32768)) return {};
    DWORD ignored{};
    DWORD n = GetFileVersionInfoSizeW(path, &ignored);
    if (!n) return {};
    std::vector<char> data(n);
    if (!GetFileVersionInfoW(path, 0, n, data.data())) return {};
    VS_FIXEDFILEINFO* info{}; UINT bytes{};
    if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&info), &bytes) ||
        bytes < sizeof(*info) || info->dwSignature != 0xFEEF04BD) return {};
    return {HIWORD(info->dwFileVersionMS), LOWORD(info->dwFileVersionMS), HIWORD(info->dwFileVersionLS)};
}
bool IsEnglish(SPBasicSuite* basic) {
    if (!basic) return false;
    const PFAppSuite6* app{};
    if (basic->AcquireSuite(kPFAppSuite, kPFAppSuiteVersion6, reinterpret_cast<const void**>(&app)) || !app) return false;
    A_char language[PF_APP_LANG_TAG_SIZE]{};
    auto err = app->PF_AppGetLanguage(language);
    basic->ReleaseSuite(kPFAppSuite, kPFAppSuiteVersion6);
    char msg[100]{};
    sprintf_s(msg, "AE UI language: %.5s", language);
    Log(msg);
    return !err && language[0] == 'e' && language[1] == 'n';
}
std::wstring ConfigValue(const wchar_t* key, const wchar_t* fallback) {
    wchar_t value[128]{};
    GetPrivateProfileStringW(L"Bridge", key, fallback, value, 128, config_path.c_str());
    return value;
}
void ReadSettings(bridge::HostVersion host) {
    wchar_t path[32768]{};
    GetModuleFileNameW(self_module, path, 32768);
    config_path = path;
    config_path = config_path.substr(0, config_path.find_last_of(L"\\/") + 1) + L"AE-Locale-Bridge.ini";
    wchar_t temp[32768]{};
    GetTempPathW(32768, temp);
    log_path = std::wstring(temp) + L"AE-Locale-Bridge-" + std::to_wstring(GetCurrentProcessId()) + L".log";
    enabled = ConfigValue(L"Enabled", L"1") == L"1";
    only_english = ConfigValue(L"OnlyEnglish", L"1") == L"1";
    const auto source = ConfigValue(L"SourceEncoding", L"auto");
    if (source == L"gbk") source_encoding = bridge::SourceEncoding::GBK;
    else if (source == L"utf8") source_encoding = bridge::SourceEncoding::UTF8;
    else if (source != L"auto") { enabled = false; Log("Invalid SourceEncoding; disabled."); }
    const auto mode = ConfigValue(L"LegacyBridge", L"auto");
    auto policy = bridge::BridgeMode::Auto;
    if (mode == L"on") policy = bridge::BridgeMode::On;
    else if (mode == L"off") policy = bridge::BridgeMode::Off;
    else if (mode != L"auto") { enabled = false; Log("Invalid LegacyBridge; disabled."); }
    legacy_bridge = bridge::UseLegacyBridge(host, policy);
    char msg[200]{};
    sprintf_s(msg, "AE Locale Bridge 0.2.1; host=%u.%u.%u; ACP=%u; OEMCP=%u; legacy=%u",
              host.major, host.minor, host.patch, GetACP(), GetOEMCP(), legacy_bridge);
    Log(msg);
}
std::string NormalizeText(std::string_view input) {
    auto output = bridge::Normalize(input, source_encoding, original_decode, original_encode);
    if (!output.empty()) ++normalized_count;
    return output;
}
const char* FixPointer(const char* input) {
    if (!input) return input;
    const auto n = strnlen_s(input, 65536);
    if (!n || n >= 65536) return input;
    auto output = NormalizeText({input, n});
    auto value = output.empty() ? std::string_view(input, n) : std::string_view(output);
    if (legacy_bridge) bridge::RegisterDisplayText(value);
    return output.empty() ? input : bridge::Intern(value);
}
template<size_t N> bool FixArray(char (&value)[N]) {
    const auto n = strnlen_s(value, N);
    if (!n || n == N) return false; // An unterminated SDK field is not a C string.
    auto output = NormalizeText({value, n});
    if (!output.empty()) {
        size_t copied = bridge::UTF8Prefix(output, N - 1);
        memcpy(value, output.data(), copied);
        value[copied] = '\0';
    }
    if (legacy_bridge) bridge::RegisterDisplayText(value);
    return !output.empty();
}
void InspectPopupSource(const char* text) {
    if (!text) return;
    const auto n = strnlen_s(text, 65536);
    if (!n || n == 65536) return;
    auto list = std::string_view(text, n);
    size_t begin{};
    for (size_t end = 0; end <= n; ++end) {
        if (end != n && list[end] != '|') continue;
        auto item = list.substr(begin, end - begin);
        if (item.find('?') != item.npos && item.find_first_not_of("? (-)\t") == item.npos) {
            if (popup_placeholder_lists.fetch_add(1, std::memory_order_relaxed) == 0)
                Log("Popup input already has placeholder-only items before host conversion; original translated text may be lost upstream.");
            break;
        }
        begin = end + 1;
    }
}
bool FixParam(PF_ParamDef* def) {
    if (!def) return false;
    bool changed = FixArray(def->PF_DEF_NAME);
    switch (def->param_type) {
        case PF_Param_POPUP:
            InspectPopupSource(def->u.pd.u.PF_DEF_NAMESPTR);
            def->u.pd.u.PF_DEF_NAMESPTR = FixPointer(def->u.pd.u.PF_DEF_NAMESPTR);
            break;
        case PF_Param_CHECKBOX: def->u.bd.u.PF_DEF_NAMEPTR = FixPointer(def->u.bd.u.PF_DEF_NAMEPTR); break;
        case PF_Param_BUTTON: def->u.button_d.u.PF_DEF_NAMESPTR = FixPointer(def->u.button_d.u.PF_DEF_NAMESPTR); break;
        case PF_Param_SLIDER: FixArray(def->u.sd.value_desc); break;
        case PF_Param_FIX_SLIDER: FixArray(def->u.fd.value_str); FixArray(def->u.fd.value_desc); break;
        case PF_Param_FLOAT_SLIDER: FixArray(def->u.fs_d.value_desc); break;
        default: break;
    }
    return changed;
}
// Host parameter lists may allocate only the active union member (SDK contract).
PF_ParamDef CopyParam(const PF_ParamDef* def) {
    PF_ParamDef copy{};
    memcpy(&copy, def, offsetof(PF_ParamDef, u));
    size_t n{};
    switch (def->param_type) {
#define COPY_SIZE(type, member) case type: n = sizeof(copy.u.member); break
        COPY_SIZE(PF_Param_LAYER, ld);
        COPY_SIZE(PF_Param_SLIDER, sd);
        COPY_SIZE(PF_Param_FIX_SLIDER, fd);
        COPY_SIZE(PF_Param_FLOAT_SLIDER, fs_d);
        COPY_SIZE(PF_Param_ANGLE, ad);
        COPY_SIZE(PF_Param_CHECKBOX, bd);
        COPY_SIZE(PF_Param_COLOR, cd);
        COPY_SIZE(PF_Param_POINT, td);
        COPY_SIZE(PF_Param_POPUP, pd);
        COPY_SIZE(PF_Param_ARBITRARY_DATA, arb_d);
        COPY_SIZE(PF_Param_PATH, path_d);
        COPY_SIZE(PF_Param_BUTTON, button_d);
        COPY_SIZE(PF_Param_POINT_3D, point3d_d);
#undef COPY_SIZE
        case PF_Param_NO_DATA:
        case PF_Param_GROUP_START:
        case PF_Param_GROUP_END: break;
        default: throw std::invalid_argument("Unsupported SDK parameter layout");
    }
    if (n) memcpy(&copy.u, &def->u, n);
    return copy;
}
template<size_t I> PF_Err FixedAdd(PF_ProgPtr ref, PF_ParamIndex index, PF_ParamDef* def) {
    auto original = reinterpret_cast<AddFn>(add_callbacks[I].original.load(std::memory_order_acquire));
    if (!original) return PF_Err_INTERNAL_STRUCT_DAMAGED;
    PF_ParamDef copy{};
    bool fixed{};
    try {
        if (def) { copy = CopyParam(def); FixParam(&copy); fixed = true; }
    } catch (...) { Log("Parameter normalization failed; forwarding original."); }
    return original(ref, index, fixed ? &copy : def);
}
void HookHostAdd(AddFn target);
PF_Err FixedUpdate(PF_ProgPtr ref, PF_ParamIndex index, const PF_ParamDef* def) {
    auto original = original_update.load(std::memory_order_acquire);
    if (!original) return PF_Err_INTERNAL_STRUCT_DAMAGED;
    PF_ParamDef copy{};
    bool fixed{};
    try {
        if (def) { copy = CopyParam(def); FixParam(&copy); fixed = true; }
    } catch (...) { Log("UI normalization failed; forwarding original."); }
    return original(ref, index, fixed ? &copy : def);
}
template<size_t I> PF_Err EffectEntry(PF_Cmd cmd, PF_InData* input, PF_OutData* out, PF_ParamDef* params[], PF_LayerDef* output, void* extra) {
    auto original = reinterpret_cast<EffectFn>(effects[I].original.load(std::memory_order_acquire));
    if (!original) return PF_Err_INTERNAL_STRUCT_DAMAGED;
    if (cmd == PF_Cmd_PARAMS_SETUP && input && input->inter.add_param) {
        try { HookHostAdd(input->inter.add_param); }
        catch (...) { Log("Host add_param hook failed."); }
        return original(cmd, input, out, params, output, extra);
    }
    // Do not mutate host-owned params during render or scan variably-sized unions.
    if ((cmd == PF_Cmd_UPDATE_PARAMS_UI || cmd == PF_Cmd_USER_CHANGED_PARAM) && input && out && params) {
        const auto result = original(cmd, input, out, params, output, extra);
        auto update = original_update.load(std::memory_order_acquire);
        if (!result && update) {
            for (PF_ParamIndex p = 1; p < input->num_params && p < 10000; ++p) {
                if (!params[p]) continue;
                try {
                    auto copy = CopyParam(params[p]);
                    if (FixArray(copy.PF_DEF_NAME)) {
                        if (!update(input->effect_ref, p, &copy)) out->out_flags |= PF_OutFlag_REFRESH_UI;
                    }
                } catch (...) { Log("Post-UI name normalization failed."); }
            }
        }
        return result;
    }
    return original(cmd, input, out, params, output, extra);
}
template<typename F> bool Install(void* target, void* detour, F publish) {
    if (!target) return false;
    // caller holds hook_mutex; publish trampoline BEFORE another thread sees detour.
    if (hooks.contains(target)) return true;
    void* trampoline{};
    if (MH_CreateHook(target, detour, &trampoline) != MH_OK) { ++failed_hooks; return false; }
    publish(trampoline);
    if (MH_EnableHook(target) != MH_OK) {
        MH_RemoveHook(target); ++failed_hooks; return false;
    }
    hooks.emplace(target, Hook{detour, trampoline});
    return true;
}
template<size_t... I> constexpr auto AddPool(std::index_sequence<I...>) {
    return std::array<AddFn, sizeof...(I)>{&FixedAdd<I>...};
}
constexpr auto add_pool = AddPool(std::make_index_sequence<64>{});
void HookHostAdd(AddFn target) {
    std::lock_guard lock(hook_mutex);
    if (hooks.contains(reinterpret_cast<void*>(target))) return;
    if (add_count == add_callbacks.size()) { Log("Host add_param wrapper pool exhausted."); return; }
    auto id = add_count++;
    if (!Install(reinterpret_cast<void*>(target), reinterpret_cast<void*>(add_pool[id]),
        [id](void* p) { add_callbacks[id].original.store(p, std::memory_order_release); }))
        Log("Host add_param hook failed.");
}
template<size_t... I> constexpr auto EffectPool(std::index_sequence<I...>) {
    return std::array<EffectFn, sizeof...(I)>{&EffectEntry<I>...};
}
constexpr auto effect_pool = EffectPool(std::make_index_sequence<effect_pool_size>{});
void HookEffect(HMODULE module, const char* name) {
    auto target = reinterpret_cast<void*>(original_get_proc(module, name));
    if (!target) return;
    std::lock_guard lock(hook_mutex);
    if (hooks.contains(target)) return;
    if (effect_count == effect_pool_size) { Log("Effect wrapper pool exhausted."); return; }
    const auto id = effect_count++;
    effects[id].module = module;
    if (!Install(target, reinterpret_cast<void*>(effect_pool[id]), [id](void* p) { effects[id].original.store(p, std::memory_order_release); }))
        Log("Effect entry hook failed.");
}
struct RegistrationContext {
    HMODULE module;
    PF_PluginDataCB2 callback2;
    PF_PluginDataCB callback1;
    RegistrationContext* previous;
};
thread_local RegistrationContext* registration_context{};
A_Err Register2(PF_PluginDataPtr ptr, const A_u_char* name, const A_u_char* match, const A_u_char* category,
                const A_u_char* entry, A_long kind, A_long major, A_long minor, A_long reserved, const A_u_char* url) {
    auto* ctx = registration_context;
    if (!ctx || !ctx->callback2) return A_Err_GENERIC;
    if (kind == 'eFKT') {
        try {
            if (entry) HookEffect(ctx->module, reinterpret_cast<const char*>(entry));
            name = reinterpret_cast<const A_u_char*>(FixPointer(reinterpret_cast<const char*>(name)));
            category = reinterpret_cast<const A_u_char*>(FixPointer(reinterpret_cast<const char*>(category)));
        } catch (...) { Log("Effect registration normalization failed."); }
    }
    return ctx->callback2(ptr, name, match, category, entry, kind, major, minor, reserved, url);
}
A_Err Register1(PF_PluginDataPtr ptr, const A_u_char* name, const A_u_char* match, const A_u_char* category,
                const A_u_char* entry, A_long kind, A_long major, A_long minor, A_long reserved) {
    auto* ctx = registration_context;
    if (!ctx || !ctx->callback1) return A_Err_GENERIC;
    if (kind == 'eFKT') {
        try {
            if (entry) HookEffect(ctx->module, reinterpret_cast<const char*>(entry));
            name = reinterpret_cast<const A_u_char*>(FixPointer(reinterpret_cast<const char*>(name)));
            category = reinterpret_cast<const A_u_char*>(FixPointer(reinterpret_cast<const char*>(category)));
        } catch (...) { Log("Effect registration normalization failed."); }
    }
    return ctx->callback1(ptr, name, match, category, entry, kind, major, minor, reserved);
}
template<size_t I, bool V2> A_Err RegistrationEntry(PF_PluginDataPtr ptr,
    std::conditional_t<V2, PF_PluginDataCB2, PF_PluginDataCB> callback,
    SPBasicSuite* basic, const char* host, const char* version) {
    auto& slot = V2 ? registration2[I] : registration1[I];
    RegistrationContext context{slot.module, nullptr, nullptr, registration_context};
    if constexpr (V2) context.callback2 = callback; else context.callback1 = callback;
    struct Restore { RegistrationContext* previous; ~Restore() { registration_context = previous; } } restore{registration_context};
    registration_context = &context;
    if constexpr (V2) {
        auto fn = reinterpret_cast<PluginDataEntryFunction2Ptr>(slot.original.load(std::memory_order_acquire));
        return fn(ptr, &Register2, basic, host, version);
    } else {
        auto fn = reinterpret_cast<PluginDataEntryFunctionPtr>(slot.original.load(std::memory_order_acquire));
        return fn(ptr, &Register1, basic, host, version);
    }
}
template<bool V2, size_t... I> constexpr auto RegistrationPool(std::index_sequence<I...>) {
    using Fn = std::conditional_t<V2, PluginDataEntryFunction2Ptr, PluginDataEntryFunctionPtr>;
    return std::array<Fn, sizeof...(I)>{&RegistrationEntry<I,V2>...};
}
constexpr auto reg2_pool = RegistrationPool<true>(std::make_index_sequence<pool_size>{});
constexpr auto reg1_pool = RegistrationPool<false>(std::make_index_sequence<pool_size>{});
template<bool V2> void HookRegistration(HMODULE module) {
    auto target = reinterpret_cast<void*>(original_get_proc(module, V2 ? "PluginDataEntryFunction2" : "PluginDataEntryFunction"));
    if (!target) return;
    std::lock_guard lock(hook_mutex);
    if (hooks.contains(target)) return;
    auto& count = V2 ? reg2_count : reg1_count;
    auto& slots = V2 ? registration2 : registration1;
    if (count == pool_size) { Log("Registration wrapper pool exhausted."); return; }
    auto id = count++;
    slots[id].module = module;
    void* wrapper{};
    if constexpr (V2) wrapper = reinterpret_cast<void*>(reg2_pool[id]); else wrapper = reinterpret_cast<void*>(reg1_pool[id]);
    Install(target, wrapper, [&](void* p) { slots[id].original.store(p, std::memory_order_release); });
}
// Win32 PiPL is a 16-bit resource version followed by little-endian PIPropertyList.
// Parse bounds before reading; no PE export scanning or AEGP/effect ABI guesses.
BOOL CALLBACK VisitPiPL(HMODULE module, LPCWSTR type, LPWSTR name, LONG_PTR) {
    auto resource = FindResourceW(module, name, type);
    auto size = SizeofResource(module, resource);
    auto data = static_cast<const unsigned char*>(LockResource(LoadResource(module, resource)));
    if (!data || size < 10) return TRUE;
    auto read32 = [&](size_t pos) { DWORD result{}; memcpy(&result, data + pos, 4); return result; };
    const size_t start = 2;
    if (read32(start) != 0) return TRUE;
    auto count = read32(start + 4);
    if (count > 4096) return TRUE;
    size_t pos = start + 8;
    bool effect{};
    std::vector<std::string> entries;
    for (DWORD i = 0; i < count; ++i) {
        if (pos > size || size - pos < 16) return TRUE;
        auto vendor = read32(pos), key = read32(pos+4), len = read32(pos+12);
        pos += 16;
        if (len > size - pos) return TRUE;
        if (vendor == '8BIM' && key == 'kind' && len == 4) effect = read32(pos) == 'eFKT';
        if (vendor == '8BIM' && key == '8664' && len && len < 1024) {
            auto entry = reinterpret_cast<const char*>(data + pos);
            auto n = strnlen_s(entry, len);
            if (n && n < len) entries.emplace_back(entry, n);
        }
        const size_t padded = (static_cast<size_t>(len) + 3) & ~size_t(3);
        if (padded > size - pos) return TRUE;
        pos += padded;
    }
    if (effect) for (const auto& entry : entries) HookEffect(module, entry.c_str());
    return TRUE;
}
void HookModule(HMODULE module) {
    if (!module || module == self_module || scanning) return;
    struct ScanGuard { ScanGuard() { scanning = true; } ~ScanGuard() { scanning = false; } } guard;
    wchar_t path[32768]{};
    if (!GetModuleFileNameW(module, path, 32768)) return;
    auto n = wcslen(path);
    if (n < 4 || _wcsicmp(path + n - 4, L".aex")) return;
    {
        std::lock_guard lock(hook_mutex);
        if (!scanned_modules.insert(module).second) return;
    }
    // Hook trampolines and interned UI pointers must outlive host-held callbacks.
    HMODULE pinned{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                            reinterpret_cast<LPCWSTR>(module), &pinned)) return;
    try {
        HookRegistration<true>(module);
        HookRegistration<false>(module);
        EnumResourceNamesW(module, L"PiPL", &VisitPiPL, 0);
    } catch (...) { Log("Module hook failed."); }
}
void EnumerateModules() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (snapshot == INVALID_HANDLE_VALUE) return;
    MODULEENTRY32W item{}; item.dwSize = sizeof(item);
    if (Module32FirstW(snapshot, &item)) do { HookModule(item.hModule); } while (Module32NextW(snapshot, &item));
    CloseHandle(snapshot);
}
HMODULE WINAPI FixedLoad(LPCWSTR path) {
    auto module = original_load(path);
    const auto error = GetLastError();
    HookModule(module);
    SetLastError(error);
    return module;
}
HMODULE WINAPI FixedLoadEx(LPCWSTR path, HANDLE file, DWORD flags) {
    auto module = original_load_ex(path, file, flags);
    const auto error = GetLastError();
    if (!(flags & (LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE | DONT_RESOLVE_DLL_REFERENCES)))
        HookModule(module);
    SetLastError(error);
    return module;
}
FARPROC WINAPI FixedGetProc(HMODULE module, LPCSTR name) {
    // Also covers modules loaded through LoadLibraryA, already-loaded DLLs and cached loads.
    HookModule(module);
    return original_get_proc(module, name);
}
int WINAPI FixedDecode(UINT cp, DWORD flags, LPCCH input, int count, LPWSTR output, int capacity) {
    try {
        auto forward = +[](UINT codepage, DWORD f, LPCCH s, int n, LPWSTR w, int c) -> int {
            return original_decode(codepage, f, s, n, w, c);
        };
        // Do not change LastError merely by inspecting an unrelated conversion.
        const auto saved = GetLastError();
        auto result = bridge::DecodeDisplayText(forward, cp, flags, input, count, output, capacity, legacy_bridge);
        if (result) SetLastError(saved);
        return result;
    } catch (...) { return original_decode(cp, flags, input, count, output, capacity); }
}
int WINAPI FixedEncode(UINT cp, DWORD flags, LPCWCH input, int count, LPSTR output,
                       int capacity, LPCCH default_char, LPBOOL used_default_char) {
    try {
        return bridge::EncodeDisplayText(original_encode, cp, flags, input, count, output,
                                          capacity, default_char, used_default_char, legacy_bridge);
    } catch (...) {
        return original_encode(cp, flags, input, count, output, capacity, default_char, used_default_char);
    }
}
void InstallHostHooks(SPBasicSuite* basic) {
    std::lock_guard lock(hook_mutex);
    if (legacy_bridge && !Install(reinterpret_cast<void*>(&MultiByteToWideChar), reinterpret_cast<void*>(&FixedDecode),
        [](void* p) { original_decode = reinterpret_cast<bridge::DecodeFn>(p); })) {
        Log("Legacy decoder hook failed; disabled.");
        enabled = false;
        return;
    }
    if (legacy_bridge && !Install(reinterpret_cast<void*>(&WideCharToMultiByte), reinterpret_cast<void*>(&FixedEncode),
        [](void* p) { original_encode = reinterpret_cast<bridge::EncodeFn>(p); }))
        Log("Legacy menu encoder hook failed; decoder bridge remains active.");
    const PF_ParamUtilsSuite3* suite{};
    if (basic && !basic->AcquireSuite(kPFParamUtilsSuite, kPFParamUtilsSuiteVersion3, reinterpret_cast<const void**>(&suite)) && suite) {
        Install(reinterpret_cast<void*>(suite->PF_UpdateParamUI), reinterpret_cast<void*>(&FixedUpdate),
            [](void* p) { original_update.store(reinterpret_cast<UpdateFn>(p), std::memory_order_release); });
        basic->ReleaseSuite(kPFParamUtilsSuite, kPFParamUtilsSuiteVersion3);
    } else Log("PF Param Utils Suite3 unavailable; dynamic UI update hook skipped.");
    Install(reinterpret_cast<void*>(&LoadLibraryW), reinterpret_cast<void*>(&FixedLoad),
        [](void* p) { original_load = reinterpret_cast<decltype(original_load)>(p); });
    Install(reinterpret_cast<void*>(&LoadLibraryExW), reinterpret_cast<void*>(&FixedLoadEx),
        [](void* p) { original_load_ex = reinterpret_cast<decltype(original_load_ex)>(p); });
    Install(reinterpret_cast<void*>(&GetProcAddress), reinterpret_cast<void*>(&FixedGetProc),
        [](void* p) { original_get_proc = reinterpret_cast<decltype(original_get_proc)>(p); });
}
A_Err OnDeath(AEGP_GlobalRefcon, AEGP_DeathRefcon) {
    char message[400]{};
    sprintf_s(message, "Shutdown; normalized=%u; decoder_hits=%llu; encoder_hits=%llu; encoder_buffer_failures=%llu; popup_placeholder_lists=%u; effects=%zu; add_callbacks=%zu; registration2=%zu; registration1=%zu; hook_failures=%u",
        normalized_count.load(), bridge::DecoderHitCount(), bridge::EncoderHitCount(), bridge::EncoderBufferFailureCount(),
        popup_placeholder_lists.load(), effect_count, add_count, reg2_count, reg1_count, failed_hooks.load());
    Log(message);
    // Keep hooks valid through shutdown: other plugins can still call them.
    // No MH_ALL_HOOKS, uninitialize or thread suspension from DllMain.
    return A_Err_NONE;
}
}

extern "C" __declspec(dllexport) A_Err EntryPointFunc(SPBasicSuite* basic, A_long, A_long,
                                                     AEGP_PluginID plugin, AEGP_GlobalRefcon*) {
    try {
        auto version = ReadHostVersion();
        ReadSettings(version);
        if (!bridge::IsSupported(version)) { Log("AE earlier than 24.0 or unknown executable version; inactive."); return A_Err_NONE; }
        if (!enabled) { Log("Disabled by configuration."); return A_Err_NONE; }
        if (only_english && !IsEnglish(basic)) { Log("English UI required or language suite unavailable; inactive."); return A_Err_NONE; }
        HMODULE pinned{};
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                          reinterpret_cast<LPCWSTR>(&EntryPointFunc), &pinned);
        auto status = MH_Initialize();
        if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) { Log("MinHook initialization failed."); return A_Err_NONE; }
        InstallHostHooks(basic);
        if (!enabled) return A_Err_NONE;
        EnumerateModules();
        const AEGP_RegisterSuite5* registration{};
        if (basic && !basic->AcquireSuite(kAEGPRegisterSuite, kAEGPRegisterSuiteVersion5, reinterpret_cast<const void**>(&registration)) && registration) {
            registration->AEGP_RegisterDeathHook(plugin, &OnDeath, nullptr);
            basic->ReleaseSuite(kAEGPRegisterSuite, kAEGPRegisterSuiteVersion5);
        }
        Log("Active; host API hooks installed.");
    } catch (...) { Log("Initialization failed; see build/host diagnostics."); }
    return A_Err_NONE;
}
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) { self_module = module; DisableThreadLibraryCalls(module); }
    return TRUE;
}
