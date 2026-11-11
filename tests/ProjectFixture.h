#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <string_view>
namespace fixture {
using Bytes = std::vector<unsigned char>;
inline void U32(Bytes& out, uint32_t n) { for (int shift=24; shift>=0; shift-=8) out.push_back(static_cast<unsigned char>(n >> shift)); }
inline Bytes Join(std::initializer_list<Bytes> parts) { Bytes out; for (const auto& p:parts) out.insert(out.end(),p.begin(),p.end()); return out; }
inline Bytes Text(std::string_view s) { return Bytes(s.begin(),s.end()); }
inline Bytes Chunk(const char* tag, Bytes body) {
    auto out=Text({tag,4}); U32(out,static_cast<uint32_t>(body.size()));
    out.insert(out.end(),body.begin(),body.end()); if(body.size()&1) out.push_back(0); return out;
}
inline Bytes List(const char* tag, Bytes body) { return Chunk("LIST", Join({Text({tag,4}),body})); }
inline Bytes Param(std::string_view name, unsigned type=10) {
    Bytes p(148); p[15]=static_cast<unsigned char>(type);
    std::copy(name.begin(),name.end(),p.begin()+16);
    return Join({Chunk("tdmn",Text(std::string("TEST-0001\0",10))),Chunk("pard",p)});
}
inline Bytes Labels(std::string_view s) { return Chunk("pdnm",Chunk("Utf8",Text(s))); }
inline Bytes Table(Bytes params, unsigned count) { Bytes n;U32(n,count);return List("parT",Join({Chunk("parn",n),params})); }
inline Bytes Project(Bytes table) { return Chunk("RIFX",Join({Text("Egg!"),List("sspc",table)})); }
inline Bytes Sample() {
    return Project(Table(Join({Param("\xB0\xEB\xBE\xB6"),Param("\xC6\xD8\xB9\xE2"),
        Param("\xC4\xA3\xCA\xBD",7),Labels("\xD5\xFD\xB3\xA3|(-|\xCC\xED\xBC\xD3|??"),
        Param("Input",7),Labels("Alpha|??")}),4));
}
}
