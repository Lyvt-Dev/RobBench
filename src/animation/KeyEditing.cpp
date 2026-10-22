#include "animation/KeyEditing.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace rb {
std::vector<CopiedKey> copyKeys(const Clip &clip, const KeySelection &selected) {
    std::vector<CopiedKey> result;
    for (const auto &ref : selected) {
        bool found = false;
        for (const auto &track : clip.tracks)
            if (track.joint == ref.joint)
                for (const auto &key : track.keys)
                    if (std::abs(key.time - ref.time) < 1e-8) {
                        result.push_back({ref.joint, key});
                        found = true;
                        break;
                    }
        if (!found)
            throw std::runtime_error("A selected keyframe no longer exists");
    }
    return result;
}
void deleteKeys(Clip &clip, const KeySelection &selected) {
    auto next = clip;
    copyKeys(clip, selected);
    for (const auto &ref : selected)
        deleteKey(next, ref.joint, ref.time);
    clip = std::move(next);
}
KeySelection transformKeyTimes(Clip &clip, const KeySelection &selected, double offset, double factor,
                               double pivot) {
    if (!std::isfinite(offset) || !std::isfinite(factor) || factor <= 0 || !std::isfinite(pivot))
        throw std::runtime_error("Timing factor must be positive and finite");
    auto keys = copyKeys(clip, selected);
    auto next = clip;
    deleteKeys(next, selected);
    KeySelection result;
    for (auto &item : keys) {
        const double time = pivot + (item.key.time - pivot) * factor + offset;
        if (!std::isfinite(time) || time < -1e-8 || time > next.duration + 1e-8)
            throw std::runtime_error("Selected keys would leave the animation range");
        item.key.time = std::clamp(time, 0., next.duration);
        for (const auto &track : next.tracks)
            if (track.joint == item.joint)
                for (const auto &key : track.keys)
                    if (std::abs(key.time - item.key.time) < 1e-8)
                        throw std::runtime_error("Keyframes would collide; no keys were moved");
        setKey(next, item.joint, item.key);
        result.insert({item.joint, item.key.time});
    }
    clip = std::move(next);
    return result;
}
void resizeClip(Clip &clip, double duration, bool scale) {
    if (!std::isfinite(duration) || duration <= 0 || duration > 86400)
        throw std::runtime_error("Invalid clip duration");
    auto next = clip;
    for (auto &track : next.tracks)
        for (auto &key : track.keys) {
            if (scale)
                key.time = key.time / clip.duration * duration;
            if (key.time > duration + 1e-8)
                throw std::runtime_error(
                    "Keys extend beyond the new duration. Enable proportional retiming or move them first.");
        }
    next.duration = duration;
    clip = std::move(next);
}
KeySelection pasteKeys(Clip &clip, const std::vector<CopiedKey> &keys, double cursor) {
    if (keys.empty())
        return {};
    double first = keys.front().key.time;
    for (const auto &item : keys)
        first = std::min(first, item.key.time);
    auto next = clip;
    KeySelection result;
    for (auto item : keys) {
        item.key.time = cursor + (item.key.time - first);
        if (item.key.time < 0 || item.key.time > next.duration)
            throw std::runtime_error("Pasted keys would leave the animation range");
        for (const auto &track : next.tracks)
            if (track.joint == item.joint)
                for (const auto &key : track.keys)
                    if (std::abs(key.time - item.key.time) < 1e-8)
                        throw std::runtime_error("Paste would overwrite an existing keyframe");
        setKey(next, item.joint, item.key);
        result.insert({item.joint, item.key.time});
    }
    clip = std::move(next);
    return result;
}
} // namespace rb
