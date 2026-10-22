#include "AEConfig.h"
#include "AE_Effect.h"
#include "AE_EffectSuites.h"
#include "AE_PluginData.h"
#include "AE_GeneralPlug.h"
#include <cstring>
#ifdef LEGACY_EFFECT
#define EFFECT_NAME OldCustomEffect
#else
#define EFFECT_NAME CustomChineseEffect
#endif
extern "C" __declspec(dllexport) PF_Err EFFECT_NAME(PF_Cmd cmd, PF_InData* in, PF_OutData*, PF_ParamDef*[], PF_LayerDef*, void*) {
    if (cmd == PF_Cmd_PARAMS_SETUP) {
        PF_ParamDef param{};
        param.param_type = PF_Param_POPUP;
        strcpy_s(param.name, "\xD6\xD0\xCE\xC4");
        param.uu.id = 1234;
        param.u.pd.num_choices = 3;
        param.u.pd.value = 2;
        param.u.pd.u.namesptr = "\xD6\xD0\xCE\xC4|Off|\xB2\xCE\xCA\xFD";
        auto err = in->inter.add_param(in->effect_ref, -1, &param);
        // Fixer must not mutate the plugin's original definition.
        if (std::strcmp(param.name, "\xD6\xD0\xCE\xC4")) return 777;
        return err;
    }
    if (cmd == PF_Cmd_USER_CHANGED_PARAM) {
        const PF_ParamUtilsSuite3* suite{};
        if (in->pica_basicP->AcquireSuite(kPFParamUtilsSuite, kPFParamUtilsSuiteVersion3, reinterpret_cast<const void**>(&suite))) return 778;
        PF_ParamDef param{};
        param.param_type = PF_Param_FLOAT_SLIDER;
        strcpy_s(param.name, "\xB2\xCE\xCA\xFD");
        param.u.fs_d.slider_min = 4;
        param.u.fs_d.slider_max = 97;
        auto err = suite->PF_UpdateParamUI(in->effect_ref, 1, &param);
        in->pica_basicP->ReleaseSuite(kPFParamUtilsSuite, kPFParamUtilsSuiteVersion3);
        return err;
    }
    return PF_Err_NONE;
}
#ifndef LEGACY_EFFECT
// Same module can contain an AEGP export. Its ABI must never be guessed as an effect.
extern "C" __declspec(dllexport) A_Err EntryPointFunc(SPBasicSuite*, A_long major, A_long minor, AEGP_PluginID, AEGP_GlobalRefcon*) {
    return major + minor;
}
extern "C" __declspec(dllexport) A_Err PluginDataEntryFunction2(PF_PluginDataPtr ptr, PF_PluginDataCB2 cb, SPBasicSuite*, const char*, const char*) {
    return cb(ptr, reinterpret_cast<const A_u_char*>("\xD6\xD0\xCE\xC4"),
        reinterpret_cast<const A_u_char*>("TEST.identity.keep"),
        reinterpret_cast<const A_u_char*>("\xB2\xCE\xCA\xFD"),
        reinterpret_cast<const A_u_char*>("CustomChineseEffect"),
        'eFKT', PF_AE_PLUG_IN_VERSION, PF_AE_PLUG_IN_SUBVERS, 8, reinterpret_cast<const A_u_char*>("https://example.invalid"));
}
extern "C" __declspec(dllexport) A_Err PluginDataEntryFunction(PF_PluginDataPtr ptr, PF_PluginDataCB cb, SPBasicSuite*, const char*, const char*) {
    return cb(ptr, reinterpret_cast<const A_u_char*>("\xD6\xD0\xCE\xC4"),
        reinterpret_cast<const A_u_char*>("TEST.identity.keep"),
        reinterpret_cast<const A_u_char*>("Category"),
        reinterpret_cast<const A_u_char*>("CustomChineseEffect"),
        'eFKT', PF_AE_PLUG_IN_VERSION, PF_AE_PLUG_IN_SUBVERS, 8);
}
#endif
