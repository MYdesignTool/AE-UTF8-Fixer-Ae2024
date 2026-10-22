// Exercise SDK-sized parameter unions without launching or installing AE.
#include "../UTF8Fixer.cpp"
#include <cstdio>
#define CHECK(expr) do { ++checks; if (!(expr)) { std::printf("FAIL line %d: %s\n", __LINE__, #expr); return 1; } } while (0)
int main() {
    int checks{};
    legacy_bridge = true;
    const char* gbk = "\xD6\xD0\xCE\xC4";
    const char* utf8 = "\xE4\xB8\xAD\xE6\x96\x87";
    PF_ParamDef def{};
    strcpy_s(def.PF_DEF_NAME, gbk);
    def.param_type = PF_Param_POPUP;
    def.uu.id = 999;
    def.u.pd.u.PF_DEF_NAMESPTR = "\xD6\xD0\xCE\xC4|Off|\xB2\xCE\xCA\xFD";
    def.u.pd.num_choices = 3; def.u.pd.value = 2;
    CHECK(FixParam(&def));
    CHECK(!std::strcmp(def.PF_DEF_NAME, utf8));
    CHECK(!std::strcmp(def.u.pd.u.PF_DEF_NAMESPTR, "\xE4\xB8\xAD\xE6\x96\x87|Off|\xE5\x8F\x82\xE6\x95\xB0"));
    CHECK(def.uu.id == 999 && def.u.pd.num_choices == 3 && def.u.pd.value == 2);
    auto stable = def.u.pd.u.PF_DEF_NAMESPTR;
    CHECK(!FixParam(&def) && def.u.pd.u.PF_DEF_NAMESPTR == stable);
    def.param_type = PF_Param_CHECKBOX; def.u.bd.u.PF_DEF_NAMEPTR = gbk;
    FixParam(&def); CHECK(!std::strcmp(def.u.bd.u.PF_DEF_NAMEPTR, utf8));
    def.param_type = PF_Param_BUTTON; def.u.button_d.u.PF_DEF_NAMESPTR = gbk;
    FixParam(&def); CHECK(!std::strcmp(def.u.button_d.u.PF_DEF_NAMESPTR, utf8));
    def = {}; def.param_type = PF_Param_SLIDER; strcpy_s(def.u.sd.value_desc, gbk);
    FixParam(&def); CHECK(!std::strcmp(def.u.sd.value_desc, utf8));
    def = {}; def.param_type = PF_Param_FIX_SLIDER;
    strcpy_s(def.u.fd.value_desc, gbk); strcpy_s(def.u.fd.value_str, gbk);
    FixParam(&def);
    CHECK(!std::strcmp(def.u.fd.value_desc, utf8) && !std::strcmp(def.u.fd.value_str, utf8));
    def = {}; def.param_type = PF_Param_FLOAT_SLIDER; strcpy_s(def.u.fs_d.value_desc, gbk);
    def.u.fs_d.value = 22; def.u.fs_d.slider_min = -50; def.u.fs_d.slider_max = 73;
    FixParam(&def);
    CHECK(!std::strcmp(def.u.fs_d.value_desc, utf8) && def.u.fs_d.value == 22 &&
          def.u.fs_d.slider_min == -50 && def.u.fs_d.slider_max == 73);
    char tiny[5] = "\xD6\xD0\xCE\xC4";
    CHECK(FixArray(tiny) && !std::strcmp(tiny, "\xE4\xB8\xAD"));
    char unterminated[4] = {'a','b','c','d'};
    CHECK(!FixArray(unterminated) && unterminated[3] == 'd');
    char latin[] = "\xC3\xA9";
    CHECK(!FixArray(latin) && !std::strcmp(latin, "\xC3\xA9"));
    CHECK(!bridge::IsRegisteredDisplayText("Off"));
    CHECK(FixPointer(nullptr) == nullptr);
    // Copy each SDK type from memory ending exactly at a PAGE_NOACCESS boundary.
    struct ParamSize { PF_ParamType type; size_t size; };
    const ParamSize sizes[]{
#define SIZE(type, member) {type, sizeof(def.u.member)}
        SIZE(PF_Param_LAYER,ld), SIZE(PF_Param_SLIDER,sd), SIZE(PF_Param_FIX_SLIDER,fd),
        SIZE(PF_Param_FLOAT_SLIDER,fs_d), SIZE(PF_Param_ANGLE,ad), SIZE(PF_Param_CHECKBOX,bd),
        SIZE(PF_Param_COLOR,cd), SIZE(PF_Param_POINT,td), SIZE(PF_Param_POPUP,pd),
        SIZE(PF_Param_ARBITRARY_DATA,arb_d), SIZE(PF_Param_PATH,path_d),
        SIZE(PF_Param_BUTTON,button_d), SIZE(PF_Param_POINT_3D,point3d_d), {PF_Param_NO_DATA,0}
#undef SIZE
    };
    SYSTEM_INFO info{}; GetSystemInfo(&info);
    auto memory = static_cast<char*>(VirtualAlloc(nullptr, info.dwPageSize*2, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE));
    CHECK(memory);
    DWORD old{}; CHECK(VirtualProtect(memory+info.dwPageSize, info.dwPageSize, PAGE_NOACCESS, &old));
    for (auto sample : sizes) {
        auto bytes = offsetof(PF_ParamDef,u)+sample.size;
        auto compact = reinterpret_cast<PF_ParamDef*>(memory+info.dwPageSize-bytes);
        std::memset(compact, 0, bytes);
        compact->param_type = sample.type; compact->uu.id = 88;
        auto copy = CopyParam(compact);
        CHECK(copy.param_type == sample.type && copy.uu.id == 88);
    }
    VirtualFree(memory, 0, MEM_RELEASE);
    def.param_type = 123456;
    bool rejected{};
    try { CopyParam(&def); } catch (const std::invalid_argument&) { rejected = true; }
    CHECK(rejected); // Unknown future SDK layouts must be forwarded intact.
    def = {};
    def.param_type = PF_Param_POPUP;
    def.u.pd.u.PF_DEF_NAMESPTR = "??|(-|????|??(-";
    def.u.pd.num_choices = 4;
    FixParam(&def);
    CHECK(popup_placeholder_lists == 1 && !std::strcmp(def.u.pd.u.PF_DEF_NAMESPTR, "??|(-|????|??(-"));
    CHECK(def.u.pd.num_choices == 4); // Diagnose upstream loss; never invent labels.
    std::printf("PASS: %d SDK parameter checks, including 14 guarded allocation layouts.\n", checks);
}
