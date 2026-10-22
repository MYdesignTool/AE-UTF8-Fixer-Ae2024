#include "AEConfig.h"
#include "AE_EffectVers.h"
resource 'PiPL'(16000) {
    {
        Kind { AEEffect },
        Name { "Legacy Chinese Test" },
        Category { "Tests" },
        CodeWin64X86 { "OldCustomEffect" },
        AE_PiPL_Version { 2, 0 },
        AE_Effect_Spec_Version { PF_PLUG_IN_VERSION, PF_PLUG_IN_SUBVERS },
        AE_Effect_Version { 65536 },
        AE_Effect_Info_Flags { 0 },
        AE_Effect_Global_OutFlags { 0 },
        AE_Effect_Global_OutFlags_2 { 0 },
        AE_Effect_Match_Name { "TEST.legacy.keep" },
        AE_Reserved_Info { 0 }
    }
};
