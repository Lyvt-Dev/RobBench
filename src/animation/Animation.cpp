#include "core/Project.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace rb {
Transform evaluate(const Track &t, double time) {
    if (t.keys.empty())
        return {};
    if (time <= t.keys.front().time)
        return t.keys.front().value;
    if (time >= t.keys.back().time)
        return t.keys.back().value;
    auto b = std::upper_bound(t.keys.begin(), t.keys.end(), time,
                              [](double x, const Keyframe &k) { return x < k.time; });
    const auto &a = *(b - 1);
    float f = static_cast<float>((time - a.time) / (b->time - a.time));
    switch (a.interpolation) {
    case Interpolation::Step:
        f = 0;
        break;
    case Interpolation::EaseIn:
        f *= f;
        break;
    case Interpolation::EaseOut:
        f = 1 - (1 - f) * (1 - f);
        break;
    case Interpolation::EaseInOut:
        f = f * f * (3 - 2 * f);
        break;
    case Interpolation::Linear:
        break;
    }
    return {glm::mix(a.value.position, b->value.position, f),
            glm::normalize(glm::slerp(a.value.rotation, b->value.rotation, f)),
            glm::mix(a.value.scale, b->value.scale, f)};
}
std::vector<Transform> evaluatePose(const Project &p, std::size_t clip, double time) {
    if (clip >= p.clips.size() || !std::isfinite(time))
        throw std::runtime_error("Invalid animation evaluation request");
    std::vector<Transform> pose(p.parts.size());
    for (std::size_t i = 0; i < p.parts.size(); ++i)
        for (const auto &t : p.clips[clip].tracks)
            if (t.joint == p.parts[i].joint) {
                pose[i] = evaluate(t, time);
                break;
            }
    return pose;
}
void setKey(Clip &c, const std::string &joint, Keyframe k) {
    if (!std::isfinite(k.time) || k.time < 0 || k.time > c.duration)
        throw std::runtime_error("Keyframe outside animation duration");
    auto it =
        std::find_if(c.tracks.begin(), c.tracks.end(), [&](const Track &t) { return t.joint == joint; });
    if (it == c.tracks.end()) {
        c.tracks.push_back({joint, {}});
        it = c.tracks.end() - 1;
    }
    auto &keys = it->keys;
    auto pos = std::lower_bound(keys.begin(), keys.end(), k.time,
                                [](const Keyframe &a, double t) { return a.time < t; });
    if (pos != keys.end() && std::abs(pos->time - k.time) < 1e-8)
        *pos = k;
    else
        keys.insert(pos, k);
}
bool deleteKey(Clip &c, const std::string &joint, double time) {
    for (auto &t : c.tracks)
        if (t.joint == joint) {
            auto old = t.keys.size();
            std::erase_if(t.keys, [&](const Keyframe &k) { return std::abs(k.time - time) < 1e-7; });
            return old != t.keys.size();
        }
    return false;
}
} // namespace rb
