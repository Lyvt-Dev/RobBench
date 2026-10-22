#include "modeling/TransformGizmo.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rb {
namespace {
bool rigid(const glm::mat4 &matrix) {
    const glm::mat3 basis(matrix);
    for (int i = 0; i < 3; ++i) {
        if (std::abs(glm::length(basis[i]) - 1) > 1e-3f)
            return false;
        for (int j = 0; j < i; ++j)
            if (std::abs(glm::dot(basis[i], basis[j])) > 1e-3f)
                return false;
    }
    return glm::determinant(basis) > 0;
}
float rounded(float value, float interval) {
    return std::round(value / interval) * interval;
}
glm::mat4 jointParentMatrix(const Project &project, const std::vector<glm::mat4> &worlds, int index) {
    const auto &part = project.parts.at(static_cast<std::size_t>(index));
    return (part.parent < 0 ? glm::mat4(1) : worlds.at(static_cast<std::size_t>(part.parent))) *
           part.c0.matrix();
}
} // namespace
glm::vec3 GizmoFrame::axis(int index) const {
    if (index < 0 || index > 2)
        throw std::out_of_range("Invalid gizmo axis");
    glm::vec3 unit(0);
    unit[index] = 1;
    return orientation * unit;
}
GizmoFrame makeGizmoFrame(const Project &project, const std::vector<Transform> &pose, int selected,
                          TransformTool tool, TransformSpace space, float radius) {
    if (selected < 0 || selected >= static_cast<int>(project.parts.size()) || radius <= 0 ||
        !std::isfinite(radius))
        throw std::invalid_argument("Invalid transform gizmo selection or radius");
    const auto worlds = worldMatrices(project, pose);
    const auto parent = jointParentMatrix(project, worlds, selected);
    auto matrix = tool == TransformTool::Resize ? worlds[selected] : parent * pose.at(selected).matrix();
    if (!rigid(matrix))
        throw std::runtime_error("Transform handles require unscaled joint ancestors and a unit joint scale");
    auto orientation = space == TransformSpace::World && tool != TransformTool::Resize
                           ? glm::quat(1, 0, 0, 0)
                           : glm::normalize(glm::quat_cast(glm::mat3(matrix)));
    return {glm::vec3(matrix[3]), orientation, radius};
}
std::optional<glm::vec3> intersectPlane(const Ray &ray, glm::vec3 origin, glm::vec3 normal) {
    const auto denominator = glm::dot(normal, ray.direction);
    if (std::abs(denominator) < 1e-5f)
        return std::nullopt;
    const auto distance = glm::dot(normal, origin - ray.origin) / denominator;
    if (distance < 0 || !std::isfinite(distance))
        return std::nullopt;
    return ray.origin + ray.direction * distance;
}
std::optional<float> axisParameter(const Ray &ray, glm::vec3 origin, glm::vec3 axis) {
    const auto direction = glm::normalize(ray.direction);
    const auto b = glm::dot(direction, axis);
    const auto denominator = 1 - b * b;
    if (denominator < 1e-4f)
        return std::nullopt;
    const auto w = origin - ray.origin;
    const auto parameter = (b * glm::dot(direction, w) - glm::dot(axis, w)) / denominator;
    const auto rayDistance = glm::dot(direction, w) + b * parameter;
    if (rayDistance < 0 || !std::isfinite(parameter))
        return std::nullopt;
    return parameter;
}
std::optional<GizmoHit> hitGizmo(const Ray &ray, const GizmoFrame &frame, TransformTool tool,
                                 float tolerance) {
    if (tool == TransformTool::Select)
        return std::nullopt;
    std::optional<GizmoHit> best;
    for (int i = 0; i < 3; ++i) {
        const auto axis = frame.axis(i);
        float error = 0;
        if (tool == TransformTool::Rotate) {
            auto point = intersectPlane(ray, frame.origin, axis);
            if (!point)
                continue;
            error = std::abs(glm::length(*point - frame.origin) - frame.radius);
        } else {
            auto t = axisParameter(ray, frame.origin, axis);
            if (!t || *t < frame.radius * .15f || *t > frame.radius * 1.1f)
                continue;
            auto point = frame.origin + *t * axis;
            auto direction = glm::normalize(ray.direction);
            auto projected = ray.origin + direction * glm::dot(point - ray.origin, direction);
            error = glm::length(projected - point);
        }
        if (error <= tolerance && (!best || error < best->distance))
            best = GizmoHit{i, error};
    }
    return best;
}
std::optional<GizmoDrag> GizmoDrag::begin(const Project &project, const std::vector<Transform> &pose,
                                          int selected, TransformTool tool, const GizmoFrame &frame, int axis,
                                          const Ray &ray) {
    if (tool == TransformTool::Select || selected < 0 || selected >= static_cast<int>(project.parts.size()) ||
        axis < 0 || axis > 2 || project.parts[selected].locked)
        return std::nullopt;
    GizmoDrag drag;
    drag.tool = tool;
    drag.frame = frame;
    drag.axisIndex = axis;
    drag.start = {pose.at(selected), project.parts[selected].size};
    const auto worlds = worldMatrices(project, pose);
    drag.jointParent = jointParentMatrix(project, worlds, selected);
    if (!rigid(drag.jointParent))
        return std::nullopt;
    drag.parentRotation = glm::normalize(glm::quat_cast(glm::mat3(drag.jointParent)));
    if (tool == TransformTool::Rotate) {
        auto point = intersectPlane(ray, frame.origin, frame.axis(axis));
        if (!point || glm::length(*point - frame.origin) < 1e-5f)
            return std::nullopt;
        drag.startDirection = glm::normalize(*point - frame.origin);
    } else {
        auto t = axisParameter(ray, frame.origin, frame.axis(axis));
        if (!t)
            return std::nullopt;
        drag.startParameter = *t;
    }
    return drag;
}
std::optional<GizmoValue> GizmoDrag::update(const Ray &ray, bool snap) const {
    GizmoValue result = start;
    const auto axis = frame.axis(axisIndex);
    if (tool == TransformTool::Rotate) {
        auto point = intersectPlane(ray, frame.origin, axis);
        if (!point || glm::length(*point - frame.origin) < 1e-5f)
            return std::nullopt;
        auto direction = glm::normalize(*point - frame.origin);
        float angle = std::atan2(glm::dot(axis, glm::cross(startDirection, direction)),
                                 glm::dot(startDirection, direction));
        if (snap)
            angle = rounded(angle, glm::radians(15.f));
        const auto delta = glm::angleAxis(angle, axis);
        result.transform.rotation =
            glm::normalize(glm::inverse(parentRotation) * delta * parentRotation * start.transform.rotation);
    } else {
        auto t = axisParameter(ray, frame.origin, axis);
        if (!t)
            return std::nullopt;
        float delta = *t - startParameter;
        if (tool == TransformTool::Move) {
            if (snap)
                delta = rounded(delta, .1f);
            auto local = glm::vec3(glm::inverse(jointParent) * glm::vec4(axis * delta, 0));
            result.transform.position = start.transform.position + local;
        } else {
            float size = start.size[axisIndex] + 2 * delta;
            if (snap)
                size = rounded(size, .1f);
            result.size[axisIndex] = std::clamp(size, .01f, 1000.f);
        }
    }
    return result;
}
} // namespace rb
