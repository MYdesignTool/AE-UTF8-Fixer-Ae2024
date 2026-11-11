#include "AEConfig.h"
#include "AE_EffectSuites.h"
#include "AE_GeneralPlug.h"
#include "ProjectFixture.h"
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <filesystem>
#include <fstream>
#define CHECK(expr) do { ++checks; if (!(expr)) { std::printf("FAIL line %d: %s, error=%lu\n",__LINE__,#expr,GetLastError());return 1;} } while(0)
namespace {
PFAppSuite6 app{};
AEGP_RegisterSuite5 registration{};
const char* ui="en_US";
PF_Err Language(A_char* out) {strcpy_s(out,PF_APP_LANG_TAG_SIZE,ui);return PF_Err_NONE;}
A_Err Death(AEGP_PluginID,AEGP_DeathHook,AEGP_DeathRefcon) {return A_Err_NONE;}
SPErr Acquire(const char* name,int32 version,const void** out) {
    if(!std::strcmp(name,kPFAppSuite)&&version==kPFAppSuiteVersion6)*out=&app;
    else if(!std::strcmp(name,kAEGPRegisterSuite)&&version==kAEGPRegisterSuiteVersion5)*out=&registration;
    else {*out=nullptr;return kSPBadParameterError;}
    return kSPNoError;
}
SPErr Release(const char*,int32) {return kSPNoError;}
bool Write(const std::filesystem::path& path,const fixture::Bytes& bytes) {
    auto h=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,0,nullptr);
    if(h==INVALID_HANDLE_VALUE)return false;
    DWORD n{};auto ok=WriteFile(h,bytes.data(),static_cast<DWORD>(bytes.size()),&n,nullptr);CloseHandle(h);
    return ok&&n==bytes.size();
}
bool ReadUnchanged(const std::filesystem::path& path,const fixture::Bytes& original,bool ansi=false) {
    const auto narrow=path.string();
    auto h=ansi?CreateFileA(narrow.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr)
               :CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if(h==INVALID_HANDLE_VALUE)return false;
    LARGE_INTEGER zero{},pos{};
    bool ok=SetFilePointerEx(h,zero,&pos,FILE_CURRENT)&&pos.QuadPart==0;
    fixture::Bytes read(original.size());DWORD n{};
    ok=ok&&ReadFile(h,read.data(),static_cast<DWORD>(read.size()),&n,nullptr)&&n==read.size()&&read==original;
    CloseHandle(h);return ok;
}
}
int wmain(int argc,wchar_t**argv) {
    int checks{};
    CHECK(argc==5||argc==6);
    const bool active=std::wstring(argv[3])==L"1";
    if(std::wstring(argv[4])==L"zh_CN")ui="zh_CN";
    const std::filesystem::path directory(argv[2]);
    std::filesystem::create_directories(directory);
    const auto widepath=directory/L"cached.AEP",ansipath=directory/L"ansi.aep",otherpath=directory/L"other.txt";
    const auto sample=fixture::Sample();
    fixture::Bytes actual;
    if(argc==6) {
        std::ifstream input(std::filesystem::path(argv[5]),std::ios::binary);
        CHECK(input.good());
        actual.assign(std::istreambuf_iterator<char>(input),{});
    }
    const auto ansi=fixture::Project(fixture::Table(fixture::Param("\xB2\xCE\xCA\xFD"),1));
    const auto other=fixture::Project(fixture::Table(fixture::Param("\xCA\xE4\xC8\xEB"),1));
    CHECK(Write(widepath,sample)&&Write(ansipath,ansi)&&Write(otherpath,other));
    app.PF_AppGetLanguage=&Language;registration.AEGP_RegisterDeathHook=&Death;
    SPBasicSuite basic{};basic.AcquireSuite=&Acquire;basic.ReleaseSuite=&Release;
    auto module=LoadLibraryW(argv[1]);CHECK(module);
    using Start=A_Err(*)(SPBasicSuite*,A_long,A_long,AEGP_PluginID,AEGP_GlobalRefcon*);
    auto start=reinterpret_cast<Start>(GetProcAddress(module,"EntryPointFunc"));
    CHECK(start&&!start(&basic,999,999,1,nullptr));
    // No effect AEX is loaded: everything comes from the cached project table.
    wchar_t text[100]{};
    CHECK(MultiByteToWideChar(1252,0,"\xB0\xEB\xBE\xB6",-1,text,100)==5);
    CHECK(ReadUnchanged(otherpath,other));
    CHECK(MultiByteToWideChar(1252,0,"\xCA\xE4\xC8\xEB",-1,text,100)==5);
    CHECK(ReadUnchanged(widepath,sample));
    CHECK(MultiByteToWideChar(1252,0,"\xB0\xEB\xBE\xB6",-1,text,100)==(active?3:5));
    CHECK(std::wstring(text)==(active?L"\x534A\x5F84":L"\xB0\xEB\xBE\xB6"));
    CHECK(MultiByteToWideChar(936,0,"\xB0\xEB\xBE\xB6",-1,text,100)==3&&std::wstring(text)==L"\x534A\x5F84");
    CHECK(MultiByteToWideChar(1252,0,"\xCC\xED\xBC\xD3",-1,text,100)==(active?3:5));
    CHECK(!active||std::wstring(text)==L"\x6DFB\x52A0");
    char menu[100]{};
    CHECK(WideCharToMultiByte(1252,0,text,-1,menu,100,nullptr,nullptr)>0);
    wchar_t displayed[100]{};
    CHECK(MultiByteToWideChar(1252,0,menu,-1,displayed,100)>0&&std::wstring(text)==displayed);
    CHECK(MultiByteToWideChar(1252,0,"Alpha|??",-1,text,100)==9&&std::wstring(text)==L"Alpha|??");
    CHECK(MultiByteToWideChar(1252,0,"\xB2\xCE\xCA\xFD",-1,text,100)==5);
    CHECK(ReadUnchanged(ansipath,ansi,true));
    CHECK(MultiByteToWideChar(1252,0,"\xB2\xCE\xCA\xFD",-1,text,100)==(active?3:5));
    CHECK(!active||std::wstring(text)==L"\x53C2\x6570");
    auto failed=CreateFileW((directory/L"absent.aep").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    CHECK(failed==INVALID_HANDLE_VALUE&&GetLastError()==ERROR_FILE_NOT_FOUND);
    if(argc==6) {
        const char* threshold="\xE3\xD0\xD6\xB5\xC6\xBD\xBB\xAC\xB6\xC8";
        CHECK(MultiByteToWideChar(1252,0,threshold,-1,text,100)==11);
        CHECK(ReadUnchanged(argv[5],actual));
        CHECK(MultiByteToWideChar(1252,0,threshold,-1,text,100)==(active?6:11));
        CHECK(!active||std::wstring(text)==L"\x9608\x503C\x5E73\x6ED1\x5EA6");
    }
    std::printf("PASS: %d missing-host checks; UI=%s; project_bridge=%d; no effect DLL; original bytes and file position preserved.\n",checks,ui,active);
}
