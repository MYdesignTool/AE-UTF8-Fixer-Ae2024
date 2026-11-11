#include "ProjectText.h"
#include <cstdint>
#include <cstring>
#include <string_view>

namespace bridge {
namespace {
constexpr size_t max_file = 256u * 1024u * 1024u;
struct Reader {
    std::span<const unsigned char> bytes;
    size_t budget = 250000;
    ProjectTextScan result;
    uint32_t U32(size_t at) const {
        return uint32_t(bytes[at]) << 24 | uint32_t(bytes[at+1]) << 16 |
               uint32_t(bytes[at+2]) << 8 | bytes[at+3];
    }
    bool Tag(size_t at, const char* tag) const { return !std::memcmp(bytes.data()+at, tag, 4); }
    struct Chunk { size_t at{}, size{}, end{}, next{}; };
    bool Next(size_t at, size_t end, Chunk& c) {
        if (!budget || end-at < 8) return false;
        --budget;
        const auto n = size_t(U32(at+4));
        if (n > end-at-8) return false;
        c = {at+8, n, at+8+n, at+8+n+(n&1)};
        return c.next <= end;
    }
    void Add(size_t at, size_t n, bool popup) {
        if (n) result.texts.push_back({std::string(reinterpret_cast<const char*>(bytes.data()+at), n), popup});
    }
    bool ParameterTable(size_t begin, size_t end) {
        if (begin == end) return true; // Later instances may omit their cache.
        Chunk c;
        if (!Next(begin, end, c) || !Tag(begin, "parn") || c.size != 4) return false;
        const auto declared = U32(c.at);
        if (declared > 10000) return false;
        size_t count{};
        bool match{}, definition{};
        uint32_t kind{};
        for (size_t p=c.next; p<end; p=c.next) {
            if (!Next(p, end, c)) return false;
            if (Tag(p, "tdmn")) {
                if (match || !c.size || c.size > 4096 ||
                    !std::memchr(bytes.data()+c.at, 0, c.size)) return false;
                match = true; definition = false;
            } else if (Tag(p, "pard")) {
                if (!match) return false;
                match = false; definition = true; ++count;
                // Other layouts are left uninterpreted, even if they contain text.
                if (c.size != 148) { definition = false; continue; }
                kind = U32(c.at+12);
                const auto* start = bytes.data()+c.at+16;
                const auto* nul = static_cast<const unsigned char*>(std::memchr(start, 0, 32));
                if (nul && kind <= 16) Add(c.at+16, size_t(nul-start), false);
            } else if (Tag(p, "pdnm") && definition && (kind == 4 || kind == 7 || kind == 15)) {
                if (c.size < 8 || !Tag(c.at, "Utf8")) return false;
                const size_t n = U32(c.at+4);
                if (n > 65535 || n > c.size-8 || c.size-8-n > 1) return false;
                // Reject embedded NUL rather than accidentally interpreting suffixes.
                if (std::memchr(bytes.data()+c.at+8, 0, n)) return false;
                Add(c.at+8, n, kind == 7);
            }
        }
        return !match && count == declared;
    }
    bool Walk(size_t begin, size_t end, unsigned depth, bool effect) {
        if (depth > 64) return false;
        Chunk c;
        for (size_t p=begin; p<end; p=c.next) {
            if (!Next(p, end, c)) return false;
            if (!Tag(p, "LIST")) continue;
            if (c.size < 4) return false;
            if (effect && Tag(c.at, "parT")) {
                if (!ParameterTable(c.at+4, c.end)) return false;
            } else if (!Walk(c.at+4, c.end, depth+1, Tag(c.at, "sspc"))) return false;
        }
        return true;
    }
};
}
ProjectTextScan ReadProjectDisplayTexts(std::span<const unsigned char> bytes) {
    if (bytes.size() < 12 || bytes.size() > max_file) return {};
    Reader r{bytes};
    if (!r.Tag(0, "RIFX") || !r.Tag(8, "Egg!")) return {};
    const size_t n = r.U32(4);
    if (n < 4 || n > bytes.size()-8 || !r.Walk(12, n+8, 0, false)) return {};
    r.result.valid = true;
    return std::move(r.result);
}
}
