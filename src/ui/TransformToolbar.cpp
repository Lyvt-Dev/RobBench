#include "renderer/Viewport.h"
#include "ui/MainWindow.h"
#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QToolBar>
#include <glm/gtc/quaternion.hpp>

namespace rb {
namespace {
class ToolShortcutFilter final : public QObject {
  public:
    using QObject::QObject;
    bool eventFilter(QObject *, QEvent *event) override {
        if (event->type() != QEvent::Shortcut)
            return false;
        auto *focus = QApplication::focusWidget();
        return qobject_cast<QLineEdit *>(focus) || qobject_cast<QAbstractSpinBox *>(focus) ||
               qobject_cast<QComboBox *>(focus) || qobject_cast<QTextEdit *>(focus) ||
               qobject_cast<QPlainTextEdit *>(focus);
    }
};
} // namespace
void MainWindow::createTransformTools() {
    addToolBarBreak();
    auto *toolbar = addToolBar("Transform tools");
    toolbar->setObjectName("transformTools");
    auto *group = new QActionGroup(this);
    group->setExclusive(true);
    auto *filter = new ToolShortcutFilter(this);
    auto *coordinates = new QComboBox;
    coordinates->addItems({"Joint local", "World"});
    coordinates->setObjectName("transformSpace");
    coordinates->setToolTip("Move and rotate in joint-local or world axes. Resize uses part-local axes.");
    const TransformTool modes[]{TransformTool::Select, TransformTool::Move, TransformTool::Rotate,
                                TransformTool::Resize};
    const QStringList labels{"Select", "Move (W)", "Rotate (E)", "Resize (R)"};
    const QStringList names{"toolSelect", "toolMove", "toolRotate", "toolResize"};
    const QStringList shortcuts{"", "W", "E", "R"};
    for (int i = 0; i < 4; ++i) {
        auto *action = toolbar->addAction(labels[i]);
        action->setObjectName(names[i]);
        action->setCheckable(true);
        action->setChecked(modes[i] == viewport->tool());
        action->setShortcut(QKeySequence(shortcuts[i]));
        action->installEventFilter(filter);
        group->addAction(action);
        auto mode = modes[i];
        connect(action, &QAction::triggered, this, [this, mode, coordinates] {
            viewport->setTool(mode);
            const bool resize = mode == TransformTool::Resize;
            if (resize)
                coordinates->setCurrentIndex(0);
            coordinates->setItemText(0, resize ? "Part local" : "Joint local");
            coordinates->setEnabled(!resize);
        });
    }
    toolbar->addSeparator();
    toolbar->addWidget(coordinates);
    connect(coordinates, &QComboBox::currentIndexChanged, this,
            [this](int i) { viewport->setSpace(i == 0 ? TransformSpace::Local : TransformSpace::World); });
    auto *snap = new QCheckBox("Snap");
    snap->setObjectName("transformSnap");
    snap->setToolTip("0.1 stud movement/size and 15 degree rotation. Hold Shift to snap temporarily.");
    toolbar->addWidget(snap);
    connect(snap, &QCheckBox::toggled, this, [this](bool enabled) { viewport->snapping = enabled; });
    toolbar->addWidget(new QLabel("   Drag a colored handle • Shift: snap • Esc: cancel"));
    viewport->editStarted = [this] {
        playing = false;
        playback.stop();
    };
    viewport->editPreviewed = [this](const GizmoValue &value) {
        refreshing = true;
        const auto euler = glm::degrees(glm::eulerAngles(value.transform.rotation));
        for (int i = 0; i < 3; ++i) {
            transforms[i]->setValue(value.transform.position[i]);
            transforms[i + 3]->setValue(euler[i]);
            dimensions[i]->setValue(value.size[i]);
        }
        refreshing = false;
    };
    viewport->editCancelled = [this] {
        if (!refreshing)
            refresh(false);
    };
    viewport->editCommitted = [this](TransformTool mode, const GizmoValue &value) {
        const int index = viewport->selected;
        if (index < 0 || index >= static_cast<int>(project.parts.size()) || project.parts[index].locked) {
            refresh(false);
            return;
        }
        auto joint = project.parts[index].joint;
        double timestamp = time;
        const auto selected = interpolation->currentIndex();
        const auto easing = selected == 0   ? Interpolation::Linear
                            : selected == 1 ? Interpolation::Step
                                            : static_cast<Interpolation>(selected);
        change(mode == TransformTool::Resize ? "Resize part with gizmo" : "Transform joint with gizmo",
               [=, this](Project &p) {
                   if (mode == TransformTool::Resize)
                       p.parts[index].size = value.size;
                   else
                       setKey(p.clips[activeClip], joint, {timestamp, value.transform, easing});
               });
    };
}
} // namespace rb
