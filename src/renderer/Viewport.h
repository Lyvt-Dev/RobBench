#pragma once
#include "core/Project.h"
#include "modeling/TransformGizmo.h"
#include <QVulkanWindow>
#include <functional>
#include <vulkan/vulkan.h>
namespace rb {
class Viewport final : public QVulkanWindow {
  public:
    Project *project{};
    std::vector<Transform> pose;
    int selected{-1};
    bool showGrid{true};
    std::function<void(int)> selectionChanged;
    std::function<void(const QString &)> failed;
    std::function<void()> frameRendered;
    std::function<void()> editStarted;
    std::function<void(const GizmoValue &)> editPreviewed;
    std::function<void(TransformTool, const GizmoValue &)> editCommitted;
    std::function<void()> editCancelled;
    bool snapping{false};
    int hoveredAxis{-1};
    Viewport();
    void setTool(TransformTool);
    TransformTool tool() const {
        return activeTool;
    }
    void setSpace(TransformSpace);
    void cancelDrag();
    bool dragging() const {
        return drag.has_value();
    }
    int dragAxis() const {
        return draggingAxis;
    }
    std::optional<GizmoFrame> gizmoFrame() const;
    Ray screenRay(QPointF) const;
    glm::vec3 partSize(std::size_t index) const;
    QVulkanWindowRenderer *createRenderer() override;
    glm::mat4 viewProjection() const;
    void focusSelected();
    void setView(int axis);
    void setOrthographic(bool enabled);
    int pick(QPointF point) const;

  protected:
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void focusOutEvent(QFocusEvent *) override;
    void wheelEvent(QWheelEvent *) override;

  private:
    TransformTool activeTool{TransformTool::Rotate};
    TransformSpace space{TransformSpace::Local};
    std::optional<GizmoDrag> drag;
    std::optional<GizmoValue> preview;
    int dragPart{-1}, draggingAxis{-1};
    QPointF previous;
    glm::vec3 target{0, 2.5f, 0};
    float yaw{.55f}, pitch{.22f}, distance{10};
    bool ortho{false};
};
} // namespace rb
