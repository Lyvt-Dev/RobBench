#include "core/Project.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stdexcept>
#include <unordered_set>

namespace rb {
glm::mat4 Transform::matrix() const {
    return glm::translate(glm::mat4(1), position) * glm::mat4_cast(glm::normalize(rotation)) *
           glm::scale(glm::mat4(1), scale);
}
static bool finite(glm::vec3 v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
static void valid(const Transform &t) {
    if (!finite(t.position) || !finite(t.scale) || glm::any(glm::lessThanEqual(t.scale, glm::vec3(0))) ||
        !std::isfinite(t.rotation.w) || !std::isfinite(t.rotation.x) || !std::isfinite(t.rotation.y) ||
        !std::isfinite(t.rotation.z) || std::abs(glm::length(t.rotation) - 1.f) > 0.001f)
        throw std::runtime_error(
            "Invalid transform: finite values, positive scale and unit quaternion required");
}
void Project::validate() const {
    if (parts.empty() || parts.size() > 10000 || clips.empty())
        throw std::runtime_error("Project must contain parts and at least one animation");
    std::unordered_set<std::string> names, joints;
    int roots = 0;
    std::size_t vertices = 0;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        const auto &p = parts[i];
        if (p.name.empty() || !names.insert(p.name).second || p.joint.empty() ||
            !joints.insert(p.joint).second)
            throw std::runtime_error("Duplicate or empty part/joint name");
        if (p.parent < -1 || p.parent >= static_cast<int>(parts.size()) || p.parent == static_cast<int>(i))
            throw std::runtime_error("Invalid parent index");
        if (p.parent == -1)
            ++roots;
        if (!finite(p.size) || glm::any(glm::lessThanEqual(p.size, glm::vec3(0))) || !finite(p.color))
            throw std::runtime_error("Invalid part dimensions or color");
        if (p.mesh) {
            vertices += p.mesh->vertices.size();
            if (vertices > 1000000 || p.mesh->vertices.empty() || p.mesh->vertices.size() % 3)
                throw std::runtime_error("Mesh exceeds triangle/vertex limits");
            for (const auto &v : p.mesh->vertices)
                if (!finite(v.position) || !finite(v.normal) || !finite(v.color) || !std::isfinite(v.uv.x) ||
                    !std::isfinite(v.uv.y) || glm::length(v.normal) < 1e-6f)
                    throw std::runtime_error("Invalid mesh vertex");
        }
        valid(p.c0);
        valid(p.c1);
        if (glm::length(p.c0.scale - glm::vec3(1)) > 0.0001f ||
            glm::length(p.c1.scale - glm::vec3(1)) > 0.0001f)
            throw std::runtime_error("Motor6D pivots cannot contain scale");
        std::vector<bool> seen(parts.size());
        for (int n = static_cast<int>(i); n != -1; n = parts[n].parent) {
            if (n < 0 || n >= static_cast<int>(parts.size()) || seen[n])
                throw std::runtime_error("Cyclic or invalid rig hierarchy");
            seen[n] = true;
        }
    }
    if (roots != 1)
        throw std::runtime_error("Rig must have exactly one root");
    std::unordered_set<std::string> clipNames;
    for (const auto &c : clips) {
        if (c.name.empty() || !clipNames.insert(c.name).second || !std::isfinite(c.duration) ||
            c.duration <= 0 || c.duration > 86400)
            throw std::runtime_error("Invalid animation name or duration");
        std::unordered_set<std::string> tracks;
        for (const auto &t : c.tracks) {
            if (!joints.contains(t.joint) || !tracks.insert(t.joint).second)
                throw std::runtime_error("Unknown or duplicate animation track");
            double previous = -1;
            for (const auto &k : t.keys) {
                if (!std::isfinite(k.time) || k.time < 0 || k.time > c.duration || k.time <= previous)
                    throw std::runtime_error("Key times must be unique, sorted and inside clip duration");
                if (static_cast<int>(k.interpolation) < 0 || static_cast<int>(k.interpolation) > 4)
                    throw std::runtime_error("Unsupported interpolation");
                valid(k.value);
                previous = k.time;
            }
        }
    }
}
std::vector<glm::mat4> worldMatrices(const Project &p, const std::vector<Transform> &pose) {
    if (pose.size() != p.parts.size())
        throw std::runtime_error("Pose size does not match rig");
    std::vector<glm::mat4> result(p.parts.size(), glm::mat4(1));
    std::vector<int> state(p.parts.size());
    std::function<void(int)> visit = [&](int i) {
        if (i < 0 || i >= static_cast<int>(p.parts.size()))
            throw std::runtime_error("Invalid hierarchy");
        if (state[i] == 2)
            return;
        if (state[i] == 1)
            throw std::runtime_error("Cyclic hierarchy");
        state[i] = 1;
        const auto &part = p.parts[i];
        auto local = part.c0.matrix() * pose[i].matrix() * glm::inverse(part.c1.matrix());
        if (part.parent >= 0) {
            visit(part.parent);
            result[i] = result[part.parent] * local;
        } else
            result[i] = local;
        state[i] = 2;
    };
    for (int i = 0; i < static_cast<int>(p.parts.size()); ++i)
        visit(i);
    return result;
}
} // namespace rb
