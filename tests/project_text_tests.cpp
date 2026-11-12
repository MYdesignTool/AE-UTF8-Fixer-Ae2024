#include "ProjectText.h"
#include "Encoding.h"
#include "ProjectFixture.h"
#include <cstdio>
#include <fstream>
#include <filesystem>
#define CHECK(expr) do { ++checks; if (!(expr)) { std::printf("FAIL line %d: %s\n",__LINE__,#expr);return 1;} } while(0)
int wmain(int argc,wchar_t**argv) {
    int checks{};
    using namespace bridge;
    const auto sample=fixture::Sample();
    auto result=ReadProjectDisplayTexts(sample);
    CHECK(result.valid && result.texts.size()==6);
    CHECK(result.texts[0].bytes=="\xB0\xEB\xBE\xB6" && !result.texts[0].popup);
    CHECK(result.texts[3].popup && result.texts[3].bytes.ends_with("|??"));
    auto suffix=sample;suffix.insert(suffix.end(),{0xFF,0,1,2});CHECK(ReadProjectDisplayTexts(suffix).valid);
    for(size_t n=0;n<sample.size();++n) CHECK(!ReadProjectDisplayTexts({sample.data(),n}).valid);
    auto broken=sample;broken[4]=0xFF;CHECK(!ReadProjectDisplayTexts(broken).valid);
    broken=sample;broken[8]='X';CHECK(!ReadProjectDisplayTexts(broken).valid);
    CHECK(!ReadProjectDisplayTexts(fixture::Project(fixture::Table(fixture::Param("Input"),2))).valid);
    CHECK(ReadProjectDisplayTexts(fixture::Project(fixture::List("parT",{}))).valid);
    auto unrelated=fixture::Chunk("RIFX",fixture::Join({fixture::Text("Egg!"),fixture::List("tdgp",fixture::Table(fixture::Param("Input"),1))}));
    CHECK(ReadProjectDisplayTexts(unrelated).valid && ReadProjectDisplayTexts(unrelated).texts.empty());
    fixture::Bytes nested=fixture::Table(fixture::Param("Input"),1);
    for(int i=0;i<70;++i) nested=fixture::List("abcd",nested);
    CHECK(!ReadProjectDisplayTexts(fixture::Project(nested)).valid);
    // Mutated lengths/tags must either fail closed or expose only bounded fields.
    uint32_t rng=0xB16749u;
    for(unsigned i=0;i<4096;++i) {
        auto fuzz=sample;
        for(unsigned j=0;j<4;++j) {
            rng=rng*1664525u+1013904223u;
            auto index=size_t(rng)%fuzz.size();
            rng=rng*1664525u+1013904223u;
            fuzz[index]=static_cast<unsigned char>(rng>>24);
        }
        auto scan=ReadProjectDisplayTexts(fuzz);
        CHECK(scan.valid||scan.texts.empty());
        for(const auto& t:scan.texts) CHECK(t.bytes.size()<=65535);
    }
    for(const auto& text:result.texts) RegisterProjectDisplayText(text.bytes,text.popup,SourceEncoding::Auto,&MultiByteToWideChar,&WideCharToMultiByte);
    CHECK(IsRegisteredProjectAlias("\xB0\xEB\xBE\xB6"));
    CHECK(!IsRegisteredProjectAlias("Unregistered text"));
    CHECK(IsRegisteredProjectMojibake(L"\xB0\xEB\xBE\xB6"));
    CHECK(!IsRegisteredProjectMojibake(L"Unregistered text"));
    wchar_t wide[64]{};
    CHECK(DecodeDisplayText(&MultiByteToWideChar,1252,0,"\xB0\xEB\xBE\xB6",-1,wide,64,true)==3);
    CHECK(std::wstring(wide)==L"\x534A\x5F84");
    CHECK(DecodeDisplayText(&MultiByteToWideChar,1252,0,"\xB0\xEB\xBE\xB6",4,wide,64,true)==2);
    CHECK(DecodeDisplayText(&MultiByteToWideChar,1252,0,"\xB0\xEB\xBE\xB6",5,wide,64,true)==3);
    SetLastError(0);CHECK(!DecodeDisplayText(&MultiByteToWideChar,1252,0,"\xB0\xEB\xBE\xB6",-1,wide,1,true));
    CHECK(GetLastError()==ERROR_INSUFFICIENT_BUFFER);
    CHECK(!RegisterProjectDisplayText("??|????",true,SourceEncoding::Auto,&MultiByteToWideChar,&WideCharToMultiByte));
    CHECK(!RegisterProjectDisplayText("\xCA\xE4\xC8\xEB",false,SourceEncoding::UTF8,&MultiByteToWideChar,&WideCharToMultiByte));
    CHECK(DecodeDisplayText(&MultiByteToWideChar,1252,0,"\xCA\xE4\xC8\xEB",-1,wide,64,true)==5);
    CHECK(DecodeDisplayText(&MultiByteToWideChar,1252,0,"??",-1,wide,64,true)==3 && std::wstring(wide)==L"??");
    CHECK(DecodeDisplayText(&MultiByteToWideChar,CP_UTF8,0,"\xB0\xEB\xBE\xB6",-1,wide,64,true)==MultiByteToWideChar(CP_UTF8,0,"\xB0\xEB\xBE\xB6",-1,wide,64));
    if(argc>1) {
        std::ifstream file(std::filesystem::path(argv[1]),std::ios::binary);
        CHECK(file.good());
        fixture::Bytes bytes((std::istreambuf_iterator<char>(file)),{});
        auto actual=ReadProjectDisplayTexts(bytes);
        CHECK(actual.valid);
        unsigned radius{},exposure{},lost{};
        for(const auto& text:actual.texts) {radius+=text.bytes=="\xB0\xEB\xBE\xB6";exposure+=text.bytes=="\xC6\xD8\xB9\xE2";lost+=text.bytes=="??|??";}
        CHECK(radius==2 && exposure==2 && lost==2);
        std::printf("Local supplied AEP: %zu cached UI fields; radius=%u; exposure=%u; lost-popup=%u.\n",actual.texts.size(),radius,exposure,lost);
    }
    std::printf("PASS: %d project cache checks.\n",checks);
}
