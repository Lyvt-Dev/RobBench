#pragma once
#include "core/Project.h"
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
    Viewport();
    QVulkanWindowRenderer *createRenderer() override;
    glm::mat4 viewProjection() const;
    void focusSelected();
    void setView(int axis);
    void setOrthographic(bool enabled);
    int pick(QPointF point) const;

  protected:
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;

  private:
    QPointF previous;
    glm::vec3 target{0, 2.5f, 0};
    float yaw{.55f}, pitch{.22f}, distance{10};
    bool ortho{false};
};
} // namespace rb
