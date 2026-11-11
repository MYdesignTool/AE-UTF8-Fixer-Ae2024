// Derived compatibility work by MYdesignTool; original project attribution in AUTHORS.md.
#pragma once
#include <span>
#include <string>
#include <vector>

namespace bridge {
struct ProjectDisplayText { std::string bytes; bool popup{}; };
struct ProjectTextScan {
    bool valid{};
    std::vector<ProjectDisplayText> texts;
};
// Read-only, bounded RIFX/Egg! reader. Only cached effect parameter UI text is
// returned, never paths, expressions, match names, values or trailing XMP.
ProjectTextScan ReadProjectDisplayTexts(std::span<const unsigned char> bytes);
}
