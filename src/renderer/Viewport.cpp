#include "renderer/Viewport.h"
#include <QCursor>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
namespace rb {
Viewport::Viewport() {
    setFlags(QVulkanWindow::PersistentResources);
}
glm::mat4 Viewport::viewProjection() const {
    glm::vec3 eye = target + distance * glm::vec3(std::cos(pitch) * std::sin(yaw), std::sin(pitch),
                                                  std::cos(pitch) * std::cos(yaw));
    float aspect = static_cast<float>(std::max(width(), 1)) / static_cast<float>(std::max(height(), 1));
    auto projection = ortho ? glm::orthoRH_ZO(-distance * .45f * aspect, distance * .45f * aspect,
                                              -distance * .45f, distance * .45f, .01f, 500.f)
                            : glm::perspectiveRH_ZO(glm::radians(45.f), aspect, .01f, 500.f);
    projection[1][1] *= -1;
    return projection * glm::lookAtRH(eye, target, glm::vec3(0, 1, 0));
}
void Viewport::focusSelected() {
    if (project && selected >= 0 && selected < static_cast<int>(pose.size())) {
        auto w = worldMatrices(*project, pose);
        target = glm::vec3(w[selected][3]);
        distance = 5;
    } else {
        target = {0, 2.5f, 0};
        distance = 10;
    }
    requestUpdate();
}
void Viewport::setView(int axis) {
    pitch = 0;
    yaw = glm::pi<float>();
    if (axis == 1)
        yaw = glm::half_pi<float>();
    if (axis == 2) {
        pitch = glm::half_pi<float>() - .001f;
    }
    if (axis == 3)
        yaw = 0;
    requestUpdate();
}
void Viewport::setOrthographic(bool e) {
    ortho = e;
    requestUpdate();
}
int Viewport::pick(QPointF point) const {
    if (!project || pose.size() != project->parts.size())
        return -1;
    auto ray = screenRay(point);
    auto origin = ray.origin;
    auto direction = ray.direction;
    float best = 1e30f;
    int selectedIndex = -1;
    auto worlds = worldMatrices(*project, pose);
    for (std::size_t i = 0; i < worlds.size(); ++i) {
        auto &part = project->parts[i];
        if (!part.visible || part.locked)
            continue;
        auto inverse = glm::inverse(glm::scale(worlds[i], partSize(i)));
        auto o = glm::vec3(inverse * glm::vec4(origin, 1));
        auto d = glm::vec3(inverse * glm::vec4(direction, 0));
        float lo = 0, hi = 1e30f;
        bool hit = true;
        for (int a = 0; a < 3; ++a) {
            if (std::abs(d[a]) < 1e-8f) {
                if (o[a] < -.5f || o[a] > .5f)
                    hit = false;
            } else {
                float t1 = (-.5f - o[a]) / d[a], t2 = (.5f - o[a]) / d[a];
                lo = std::max(lo, std::min(t1, t2));
                hi = std::min(hi, std::max(t1, t2));
            }
        }
        if (hit && lo <= hi && part.mesh) {
            float meshHit = 1e30f;
            for (std::size_t v = 0; v + 2 < part.mesh->vertices.size(); v += 3) {
                auto a = part.mesh->vertices[v].position, b = part.mesh->vertices[v + 1].position,
                     c = part.mesh->vertices[v + 2].position;
                auto edge1 = b - a, edge2 = c - a, h = glm::cross(d, edge2);
                float det = glm::dot(edge1, h);
                if (std::abs(det) < 1e-8f)
                    continue;
                auto s = o - a;
                float u = glm::dot(s, h) / det;
                if (u < 0 || u > 1)
                    continue;
                auto q = glm::cross(s, edge1);
                float bary = glm::dot(d, q) / det;
                if (bary < 0 || u + bary > 1)
                    continue;
                float distance = glm::dot(edge2, q) / det;
                if (distance >= 0)
                    meshHit = std::min(meshHit, distance);
            }
            hit = meshHit < 1e30f;
            lo = meshHit;
        }
        if (hit && lo <= hi && lo < best) {
            best = lo;
            selectedIndex = static_cast<int>(i);
        }
    }
    return selectedIndex;
}

void Viewport::setTool(TransformTool value) {
    cancelDrag();
    activeTool = value;
    hoveredAxis = -1;
    requestUpdate();
}
void Viewport::setSpace(TransformSpace value) {
    cancelDrag();
    space = value;
    hoveredAxis = -1;
    requestUpdate();
}
void Viewport::cancelDrag() {
    if (!drag)
        return;
    if (dragPart >= 0 && dragPart < static_cast<int>(pose.size()))
        pose[dragPart] = drag->initial().transform;
    drag.reset();
    preview.reset();
    dragPart = -1;
    draggingAxis = -1;
    setMouseGrabEnabled(false);
    requestUpdate();
    if (editCancelled)
        editCancelled();
}
std::optional<GizmoFrame> Viewport::gizmoFrame() const {
    if (!project || activeTool == TransformTool::Select || selected < 0 ||
        selected >= static_cast<int>(project->parts.size()) || project->parts[selected].locked ||
        pose.size() != project->parts.size())
        return std::nullopt;
    try {
        auto frame = makeGizmoFrame(*project, pose, selected, activeTool, space, 1);
        auto point = viewProjection() * glm::vec4(frame.origin, 1);
        if (!ortho && point.w <= .01f)
            return std::nullopt;
        const float perPixel = ortho ? distance * .9f / std::max(height(), 1)
                                     : 2 * std::tan(glm::radians(22.5f)) * point.w / std::max(height(), 1);
        frame.radius = std::clamp(perPixel * 80.f, .001f, 10000.f);
        return frame;
    } catch (const std::exception &) {
        return std::nullopt;
    }
}
glm::vec3 Viewport::partSize(std::size_t i) const {
    if (preview && dragPart == static_cast<int>(i))
        return preview->size;
    return project->parts.at(i).size;
}
Ray Viewport::screenRay(QPointF point) const {
    const auto inv = glm::inverse(viewProjection());
    float x = 2 * static_cast<float>(point.x()) / std::max(width(), 1) - 1;
    float y = 2 * static_cast<float>(point.y()) / std::max(height(), 1) - 1;
    glm::vec4 near = inv * glm::vec4(x, y, 0, 1), far = inv * glm::vec4(x, y, 1, 1);
    near /= near.w;
    far /= far.w;
    return {glm::vec3(near), glm::normalize(glm::vec3(far - near))};
}
void Viewport::mousePressEvent(QMouseEvent *e) {
    previous = e->position();
    if (e->button() == Qt::LeftButton) {
        if (auto frame = gizmoFrame())
            if (auto hit = hitGizmo(screenRay(e->position()), *frame, activeTool, frame->radius * .1f)) {
                auto next = GizmoDrag::begin(*project, pose, selected, activeTool, *frame, hit->axis,
                                             screenRay(e->position()));
                if (next) {
                    if (editStarted)
                        editStarted();
                    drag = std::move(next);
                    dragPart = selected;
                    draggingAxis = hit->axis;
                    setMouseGrabEnabled(true);
                    requestActivate();
                    requestUpdate();
                    return;
                }
            }
        int i = pick(e->position());
        if (selectionChanged)
            selectionChanged(i);
    }
    requestActivate();
}
void Viewport::mouseMoveEvent(QMouseEvent *e) {
    auto d = e->position() - previous;
    previous = e->position();
    if (drag) {
        if (auto value = drag->update(screenRay(e->position()),
                                      snapping || e->modifiers().testFlag(Qt::ShiftModifier))) {
            preview = *value;
            pose[dragPart] = value->transform;
            if (editPreviewed)
                editPreviewed(*value);
            requestUpdate();
        }
        return;
    }
    if (e->buttons() & Qt::RightButton) {
        yaw -= static_cast<float>(d.x()) * .008f;
        pitch = std::clamp(pitch + static_cast<float>(d.y()) * .008f, -1.55f, 1.55f);
        requestUpdate();
    } else if (e->buttons() & Qt::MiddleButton) {
        glm::vec3 right{std::cos(yaw), 0, -std::sin(yaw)};
        target -= right * static_cast<float>(d.x()) * distance * .0015f;
        target.y += static_cast<float>(d.y()) * distance * .0015f;
        requestUpdate();
    } else {
        int axis = -1;
        if (auto frame = gizmoFrame())
            if (auto hit = hitGizmo(screenRay(e->position()), *frame, activeTool, frame->radius * .1f))
                axis = hit->axis;
        if (axis != hoveredAxis) {
            hoveredAxis = axis;
            setCursor(axis < 0 ? Qt::ArrowCursor : Qt::CrossCursor);
            requestUpdate();
        }
    }
}
void Viewport::mouseReleaseEvent(QMouseEvent *e) {
    if (e->button() != Qt::LeftButton || !drag)
        return;
    const auto value = preview.value_or(drag->initial());
    const auto mode = activeTool;
    const auto initial = drag->initial();
    const bool changed = glm::length(value.size - initial.size) > 1e-5f ||
                         glm::length(value.transform.position - initial.transform.position) > 1e-5f ||
                         std::min(glm::length(value.transform.rotation - initial.transform.rotation),
                                  glm::length(value.transform.rotation + initial.transform.rotation)) > 1e-6f;
    drag.reset();
    preview.reset();
    dragPart = -1;
    draggingAxis = -1;
    setMouseGrabEnabled(false);
    if (changed) {
        if (editCommitted)
            editCommitted(mode, value);
    } else if (editCancelled)
        editCancelled();
    requestUpdate();
}
void Viewport::keyPressEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_Escape && drag) {
        cancelDrag();
        e->accept();
        return;
    }
    QVulkanWindow::keyPressEvent(e);
}
void Viewport::focusOutEvent(QFocusEvent *e) {
    cancelDrag();
    QVulkanWindow::focusOutEvent(e);
}
void Viewport::wheelEvent(QWheelEvent *e) {
    if (drag)
        return;
    distance = std::clamp(distance * std::exp(-static_cast<float>(e->angleDelta().y()) * .001f), .3f, 200.f);
    requestUpdate();
}
} // namespace rb
