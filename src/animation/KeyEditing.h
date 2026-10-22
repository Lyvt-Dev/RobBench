#pragma once
#include "core/Project.h"
#include <compare>
#include <set>
namespace rb {
struct KeyRef {
    std::string joint;
    double time{};
    auto operator<=>(const KeyRef &) const = default;
};
using KeySelection = std::set<KeyRef>;
struct CopiedKey {
    std::string joint;
    Keyframe key;
};
KeySelection transformKeyTimes(Clip &, const KeySelection &, double offset, double factor = 1,
                               double pivot = 0);
void deleteKeys(Clip &, const KeySelection &);
void resizeClip(Clip &, double duration, bool scaleKeys);
std::vector<CopiedKey> copyKeys(const Clip &, const KeySelection &);
KeySelection pasteKeys(Clip &, const std::vector<CopiedKey> &, double cursor);
} // namespace rb
