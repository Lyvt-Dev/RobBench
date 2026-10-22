#pragma once
#include "core/Project.h"
#include <optional>

namespace rb {
enum class TransformTool { Select, Move, Rotate, Resize };
enum class TransformSpace { Local, World };
struct Ray {
    glm::vec3 origin{0};
    glm::vec3 direction{0, 0, -1};
};
struct GizmoFrame {
    glm::vec3 origin{0};
    glm::quat orientation{1, 0, 0, 0};
    float radius{1};
    glm::vec3 axis(int index) const;
};
struct GizmoHit {
    int axis{-1};
    float distance{0};
};
struct GizmoValue {
    Transform transform;
    glm::vec3 size{1};
};
GizmoFrame makeGizmoFrame(const Project &, const std::vector<Transform> &pose, int selected, TransformTool,
                          TransformSpace, float radius);
std::optional<glm::vec3> intersectPlane(const Ray &, glm::vec3 origin, glm::vec3 normal);
std::optional<float> axisParameter(const Ray &, glm::vec3 origin, glm::vec3 axis);
std::optional<GizmoHit> hitGizmo(const Ray &, const GizmoFrame &, TransformTool, float tolerance);

// A drag owns its initial pose; repeated mouse moves never accumulate rounding error
// or mutate the document. The caller commits its final value as a single command.
class GizmoDrag {
  public:
    static std::optional<GizmoDrag> begin(const Project &, const std::vector<Transform> &, int selected,
                                          TransformTool, const GizmoFrame &, int axis, const Ray &);
    std::optional<GizmoValue> update(const Ray &, bool snap) const;
    const GizmoValue &initial() const {
        return start;
    }

  private:
    TransformTool tool{TransformTool::Select};
    GizmoFrame frame{};
    int axisIndex{0};
    glm::mat4 jointParent{1};
    glm::quat parentRotation{1, 0, 0, 0};
    GizmoValue start;
    float startParameter{0};
    glm::vec3 startDirection{0};
};
} // namespace rb
