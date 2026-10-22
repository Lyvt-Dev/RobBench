#include "io/Serialization.h"
#include "renderer/Viewport.h"
#include "ui/MainWindow.h"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScreen>
#include <QShortcutEvent>
#include <cmath>
#include <memory>
#include <spdlog/spdlog.h>
namespace rb {
void MainWindow::startSmokeTest(const QString &dir, const QString &rigType) {
    auto pathOf = [](const QString &text) { return std::filesystem::path(text.toStdU16String()); };
    project = makeRig(rigType.toStdString());
    activeClip = 0;
    time = 0;
    history.clear();
    refresh();
    const int arm = rigType == "R6" ? 3 : 4;
    const int pickedArm = rigType == "R6" ? 4 : 10;
    QDir().mkpath(dir);
    select(arm);
    insertKey();
    seek(1);
    transforms[3]->setValue(70);
    editTransform();
    const auto keyed = projectJson(project);
    history.undo();
    if (projectJson(project) == keyed)
        throw std::runtime_error("Smoke: rotation undo failed");
    history.redo();
    if (projectJson(project) != keyed)
        throw std::runtime_error("Smoke: rotation redo failed");
    seek(2);
    transforms[3]->setValue(0);
    editTransform();
    saveProject(project, pathOf(dir + "/smoke.robbench"));
    project = loadProject(pathOf(dir + "/smoke.robbench"));
    exportAnimation(project, 0, pathOf(dir + "/smoke.rbanim"));
    seek(.5);
    if (std::abs(transforms[3]->value() - 35) > 0.01)
        throw std::runtime_error("Smoke: interpolated inspector pose is wrong");
    const auto beforeCamera = viewport->viewProjection();
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(200, 200), QPointF(200, 200), Qt::RightButton,
                      Qt::RightButton, Qt::NoModifier);
    QApplication::sendEvent(viewport, &press);
    QMouseEvent move(QEvent::MouseMove, QPointF(240, 210), QPointF(240, 210), Qt::NoButton, Qt::RightButton,
                     Qt::NoModifier);
    QApplication::sendEvent(viewport, &move);
    if (viewport->viewProjection() == beforeCamera)
        throw std::runtime_error("Smoke: orbit camera did not respond");
    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(240, 210), QPointF(240, 210), Qt::RightButton,
                        Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(viewport, &release);
    auto frames = std::make_shared<int>(0);
    viewport->frameRendered = [this, dir, frames, pickedArm] {
        if (++*frames == 3)
            QTimer::singleShot(100, this, [this, dir, pickedArm] {
                try {
                    runGizmoSmokeTest();
                } catch (const std::exception &e) {
                    spdlog::error("Gizmo smoke: {}", e.what());
                    QApplication::exit(7);
                    return;
                }
                auto worlds = worldMatrices(project, viewport->pose);
                auto clip = viewport->viewProjection() * worlds[pickedArm] * glm::vec4(0, 0, 0, 1);
                auto ndc = glm::vec3(clip) / clip.w;
                QPointF pixel((ndc.x + 1) * .5 * viewport->width(), (ndc.y + 1) * .5 * viewport->height());
                if (viewport->pick(pixel) != pickedArm) {
                    spdlog::error("Smoke: viewport picking failed");
                    QApplication::exit(4);
                    return;
                }
                auto full = screen()->grabWindow(winId()).toImage();
                auto origin = viewport->mapToGlobal(QPoint(0, 0)) - mapToGlobal(QPoint(0, 0));
                auto image = full.copy(QRect(origin, viewport->size()));
                if (image.isNull() || !image.save(dir + "/viewport.png")) {
                    spdlog::error("Viewport capture failed");
                    QApplication::exit(3);
                    return;
                }
                full.save(dir + "/editor.png");
                togglePlay();
                const double initial = time;
                QTimer::singleShot(100, this, [this, initial] {
                    if (!playing || time <= initial) {
                        spdlog::error("Smoke: playback did not advance");
                        QApplication::exit(5);
                        return;
                    }
                    togglePlay();
                    resize(1240, 820);
                    QTimer::singleShot(100, this, [] {
                        spdlog::info("Editor workflow, Vulkan frames, playback and resize completed");
                        QApplication::exit(0);
                    });
                });
            });
        else if (*frames < 3)
            viewport->requestUpdate();
    };
    QTimer::singleShot(20000, this, [] {
        spdlog::error("Vulkan smoke test timed out");
        QApplication::exit(2);
    });
    viewport->requestUpdate();
}

void MainWindow::runGizmoSmokeTest() {
    auto require = [](bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    };
    const int selected = viewport->selected;
    const auto baseline = projectJson(project);
    const int initialHistory = history.index();
    auto point = [this](glm::vec3 world) {
        auto clip = viewport->viewProjection() * glm::vec4(world, 1);
        auto p = glm::vec3(clip) / clip.w;
        return QPointF((p.x + 1) * .5 * viewport->width(), (p.y + 1) * .5 * viewport->height());
    };
    auto mouse = [this](QEvent::Type type, QPointF position, Qt::MouseButton button, Qt::MouseButtons buttons,
                        Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        QMouseEvent event(type, position, QPointF(viewport->mapToGlobal(position.toPoint())), button, buttons,
                          modifiers);
        QApplication::sendEvent(viewport, &event);
    };
    auto mode = [this, &require](const char *name, TransformTool tool) {
        auto *action = findChild<QAction *>(name);
        require(action, "Missing tool action");
        action->trigger();
        require(viewport->tool() == tool, "Tool action did not change viewport mode");
    };
    auto axisHandle = [this, &point, &require](const GizmoFrame &frame, int axis) {
        const float angles[]{.3f, .6f, .9f, 1.2f, 1.8f, 2.4f};
        for (float a : angles) {
            auto world = viewport->tool() == TransformTool::Rotate
                             ? frame.origin + frame.radius * (std::cos(a) * frame.axis((axis + 1) % 3) +
                                                              std::sin(a) * frame.axis((axis + 2) % 3))
                             : frame.origin + frame.radius * .8f * frame.axis(axis);
            auto pixel = point(world);
            auto hit = hitGizmo(viewport->screenRay(pixel), frame, viewport->tool(), frame.radius * .1f);
            if (hit && hit->axis == axis)
                return world;
        }
        require(false, "Cannot hit a rendered transform handle");
        return frame.origin;
    };
    auto begin = [this, &mouse, &point, &require](glm::vec3 world) {
        mouse(QEvent::MouseButtonPress, point(world), Qt::LeftButton, Qt::LeftButton);
        require(viewport->dragging(), "Pressing a gizmo did not start a drag");
    };
    auto *coordinates = findChild<QComboBox *>("transformSpace");
    require(coordinates, "Missing coordinate selector");
    // Finishing an unchanged inspector edit must not add a key just because focus moved.
    editTransform();
    require(projectJson(project) == baseline && history.index() == initialHistory,
            "Unchanged inspector edit created a key");
    const auto angle = transforms[3]->value();
    transforms[3]->setValue(angle + .001);
    editTransform();
    require(history.index() == initialHistory + 1, "A fine inspector rotation was incorrectly discarded");
    history.undo();
    require(projectJson(project) == baseline, "Fine rotation undo failed");
    mode("toolMove", TransformTool::Move);
    coordinates->setCurrentIndex(1);
    auto frame = viewport->gizmoFrame();
    require(frame.has_value(), "Move gizmo missing");
    auto start = axisHandle(*frame, 0);
    auto target = start + glm::vec3(.26f, 0, 0);
    auto before = worldMatrices(project, viewport->pose);
    begin(start);
    mouse(QEvent::MouseButtonRelease, point(start), Qt::LeftButton, Qt::NoButton);
    require(projectJson(project) == baseline && history.index() == initialHistory,
            "Clicking a handle without dragging created a key");
    begin(start);
    mouse(QEvent::MouseMove, point(target), Qt::NoButton, Qt::LeftButton, Qt::ShiftModifier);
    require(projectJson(project) == baseline && history.index() == initialHistory,
            "Drag preview changed the document or undo stack");
    auto previewWorld = worldMatrices(project, viewport->pose);
    require(glm::length(glm::vec3(previewWorld[selected][3] - before[selected][3]) - glm::vec3(.3f, 0, 0)) <
                .002f,
            "Move preview or snapping is incorrect");
    mouse(QEvent::MouseButtonRelease, point(target), Qt::LeftButton, Qt::NoButton);
    require(!viewport->dragging() && history.index() == initialHistory + 1,
            "A completed drag must create exactly one undo command");
    const auto moved = projectJson(project);
    require(moved != baseline, "Move was not committed");
    history.undo();
    require(projectJson(project) == baseline, "Gizmo move undo failed");
    history.redo();
    require(projectJson(project) == moved, "Gizmo move redo failed");
    history.undo();

    frame = viewport->gizmoFrame();
    require(frame.has_value(), "Move gizmo missing after undo");
    start = axisHandle(*frame, 0);
    begin(start);
    mouse(QEvent::MouseMove, point(start + glm::vec3(.4f, 0, 0)), Qt::NoButton, Qt::LeftButton);
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(viewport, &escape);
    require(!viewport->dragging() && projectJson(project) == baseline && history.index() == initialHistory,
            "Esc did not cancel the drag atomically");
    require(glm::length(viewport->pose[selected].position -
                        evaluatePose(project, activeClip, time)[selected].position) < 1e-5f,
            "Cancelled preview was not restored");

    mode("toolRotate", TransformTool::Rotate);
    coordinates->setCurrentIndex(1);
    frame = viewport->gizmoFrame();
    require(frame.has_value(), "Rotate gizmo missing");
    start = axisHandle(*frame, 2);
    target = frame->origin + glm::angleAxis(.3f, glm::vec3(0, 0, 1)) * (start - frame->origin);
    auto original = viewport->pose[selected].rotation;
    begin(start);
    mouse(QEvent::MouseMove, point(target), Qt::NoButton, Qt::LeftButton, Qt::ShiftModifier);
    auto expected = glm::angleAxis(glm::radians(15.f), glm::vec3(0, 0, 1)) * original;
    require(std::abs(glm::dot(expected, viewport->pose[selected].rotation)) > .99999f,
            "World rotation preview or angle snap failed");
    require(projectJson(project) == baseline, "Rotation preview mutated the document");
    mouse(QEvent::MouseButtonRelease, point(target), Qt::LeftButton, Qt::NoButton);
    require(history.index() == initialHistory + 1, "Rotation drag did not create a single command");
    history.undo();
    require(projectJson(project) == baseline, "Rotation undo failed");

    mode("toolResize", TransformTool::Resize);
    require(!coordinates->isEnabled() && coordinates->currentIndex() == 0, "Resize must use part-local axes");
    frame = viewport->gizmoFrame();
    require(frame.has_value(), "Resize gizmo missing");
    start = axisHandle(*frame, 0);
    target = start + frame->axis(0) * .2f;
    const auto size = project.parts[selected].size;
    begin(start);
    mouse(QEvent::MouseMove, point(target), Qt::NoButton, Qt::LeftButton);
    require(projectJson(project) == baseline, "Resize preview mutated the document");
    require(std::abs(viewport->partSize(selected).x - size.x - .4f) < .002f,
            "Resize preview geometry did not change");
    mouse(QEvent::MouseButtonRelease, point(target), Qt::LeftButton, Qt::NoButton);
    require(history.index() == initialHistory + 1 &&
                std::abs(project.parts[selected].size.x - size.x - .4f) < .002f,
            "Resize commit failed");
    require(projectJson(project).at("clips") == baseline.at("clips"),
            "Resize incorrectly created animation scale keys");
    history.undo();
    require(projectJson(project) == baseline, "Resize undo failed");

    mode("toolRotate", TransformTool::Rotate);
    transforms[0]->setFocus();
    QApplication::processEvents();
    require(QApplication::focusWidget() == transforms[0] ||
                transforms[0]->isAncestorOf(QApplication::focusWidget()),
            "Cannot focus inspector for shortcut test");
    QShortcutEvent shortcut(QKeySequence("W"), 0, false);
    QApplication::sendEvent(findChild<QAction *>("toolMove"), &shortcut);
    require(viewport->tool() == TransformTool::Rotate,
            "Tool shortcut activated while typing in the inspector");
    viewport->requestActivate();
    require(projectJson(project) == baseline && history.index() == initialHistory,
            "Gizmo workflow did not restore its baseline");
    spdlog::info(
        "Gizmo move/rotate/resize, snapping, preview isolation, undo/redo, Esc and typing checks passed");
}
} // namespace rb
