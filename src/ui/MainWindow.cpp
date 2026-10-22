#include "ui/MainWindow.h"
#include "io/ModelImporter.h"
#include "io/Serialization.h"
#include "renderer/Viewport.h"
#include "ui/Timeline.h"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QToolBar>
#include <QTreeWidget>
#include <QUndoCommand>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <spdlog/spdlog.h>
namespace rb {
namespace {
std::filesystem::path pathOf(const QString &s) {
    return std::filesystem::path(s.toStdU16String());
}
class EditCommand final : public QUndoCommand {
    Project &target;
    Project before, after;
    std::function<void()> notify;

  public:
    EditCommand(Project &p, Project next, QString text, std::function<void()> f)
        : target(p), before(p), after(std::move(next)), notify(std::move(f)) {
        setText(text);
    }
    void undo() override {
        target = before;
        notify();
    }
    void redo() override {
        target = after;
        notify();
    }
};
bool typing() {
    auto *f = QApplication::focusWidget();
    return qobject_cast<QLineEdit *>(f) || qobject_cast<QAbstractSpinBox *>(f) ||
           qobject_cast<QComboBox *>(f);
}
} // namespace
MainWindow::MainWindow(QVulkanInstance *instance) : project(makeRig("R15")) {
    setWindowTitle("RobBench — Untitled");
    resize(1360, 900);
    setDockNestingEnabled(true);
    viewport = new Viewport;
    viewport->setVulkanInstance(instance);
    viewport->setSampleCount(QSettings().value("viewportMsaa", true).toBool() &&
                                     viewport->supportedSampleCounts().contains(4)
                                 ? 4
                                 : 1);
    viewport->project = &project;
    setCentralWidget(QWidget::createWindowContainer(viewport));
    viewport->selectionChanged = [this](int i) { select(i); };
    viewport->failed = [this](const QString &message) {
        spdlog::error("{}", message.toStdString());
        QTimer::singleShot(0, this,
                           [this, message] { QMessageBox::critical(this, "Vulkan renderer", message); });
    };
    createPanels();
    createMenus();
    setupTimelineActions();
    createTransformTools();
    connect(&history, &QUndoStack::cleanChanged, this, [this] { setWindowModified(!history.isClean()); });
    setWindowTitle("RobBench — Untitled[*]");
    connect(&playback, &QTimer::timeout, this, [this] {
        double delta = clock.restart() / 1000.;
        double next = time + delta;
        const auto &c = project.clips[activeClip];
        if (next > c.duration) {
            if (c.loop)
                next = std::fmod(next, c.duration);
            else {
                next = c.duration;
                playing = false;
                playback.stop();
            }
        }
        seek(next);
    });
    playback.setInterval(16);
    connect(&autosave, &QTimer::timeout, this, [this] {
        if (history.isClean())
            return;
        try {
            saveProject(project, pathOf(recoveryPath()));
            statusBar()->showMessage("Recovery snapshot saved", 3000);
        } catch (const std::exception &e) {
            spdlog::error("Autosave: {}", e.what());
        }
    });
    QSettings settings;
    autosave.start(std::clamp(settings.value("autosaveSeconds", 120).toInt(), 10, 3600) * 1000);
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("layout").toByteArray(), 1);
    refresh();
    QTimer::singleShot(0, this, [this] {
        if (QFileInfo::exists(recoveryPath()) &&
            QMessageBox::question(this, "Recover project", "A recovery snapshot exists. Open it?") ==
                QMessageBox::Yes)
            openProject(recoveryPath());
    });
}
MainWindow::~MainWindow() {
    playback.stop();
    autosave.stop();
    refreshing = true;
    viewport->cancelDrag();
    viewport->project = nullptr;
    viewport->selectionChanged = {};
    viewport->failed = {};
    viewport->frameRendered = {};
    viewport->editStarted = {};
    viewport->editPreviewed = {};
    viewport->editCommitted = {};
    viewport->editCancelled = {};
}
void MainWindow::createPanels() {
    auto dock = [this](QString title, QString id, QWidget *widget, Qt::DockWidgetArea area) {
        auto *d = new QDockWidget(title, this);
        d->setObjectName(id);
        d->setWidget(widget);
        addDockWidget(area, d);
        return d;
    };
    outliner = new QTreeWidget;
    outliner->setHeaderLabels({"Scene / Rig", "Visible", "Lock"});
    outliner->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    outliner->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    outliner->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    outliner->setMinimumWidth(340);
    outliner->setIndentation(14);
    outliner->setSelectionMode(QAbstractItemView::SingleSelection);
    dock("Outliner", "outliner", outliner, Qt::LeftDockWidgetArea);
    connect(outliner, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *item) {
        if (!refreshing)
            select(item ? item->data(0, Qt::UserRole).toInt() : -1);
    });
    connect(outliner, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item, int col) {
        if (refreshing || col == 0)
            return;
        int i = item->data(0, Qt::UserRole).toInt();
        bool value = item->checkState(col) == Qt::Checked;
        change(col == 1 ? "Change visibility" : "Change lock", [=](Project &p) {
            if (col == 1)
                p.parts[i].visible = value;
            else
                p.parts[i].locked = value;
        });
    });
    auto *inspector = new QWidget;
    auto *form = new QFormLayout(inspector);
    selectedLabel = new QLabel("Select a body part");
    selectedLabel->setWordWrap(true);
    form->addRow(selectedLabel);
    QStringList labels{"Position X", "Position Y", "Position Z", "Rotation X", "Rotation Y", "Rotation Z"};
    for (int i = 0; i < 6; ++i) {
        transforms[i] = new QDoubleSpinBox;
        transforms[i]->setRange(i < 3 ? -10000 : -3600, i < 3 ? 10000 : 3600);
        transforms[i]->setDecimals(3);
        transforms[i]->setSingleStep(i < 3 ? .1 : 5);
        form->addRow(labels[i], transforms[i]);
        connect(transforms[i], &QDoubleSpinBox::editingFinished, this, [this] { editTransform(); });
    }
    form->addRow(new QLabel("Joint edits create a key at the current time."));
    for (int i = 0; i < 3; ++i) {
        dimensions[i] = new QDoubleSpinBox;
        dimensions[i]->setRange(.01, 1000);
        dimensions[i]->setDecimals(3);
        form->addRow(QString("Part size %1").arg("XYZ"[i]), dimensions[i]);
        connect(dimensions[i], &QDoubleSpinBox::editingFinished, this, [this, i] {
            if (refreshing || viewport->selected < 0)
                return;
            int n = viewport->selected;
            float v = static_cast<float>(dimensions[i]->value());
            if (std::abs(v - project.parts[n].size[i]) < 1e-5f)
                return;
            change("Resize part", [=](Project &p) {
                if (!p.parts[n].locked)
                    p.parts[n].size[i] = v;
            });
        });
    }
    auto *propsDock = dock("Properties", "properties", inspector, Qt::RightDockWidgetArea);
    propsDock->setMinimumWidth(260);
    auto *animation = new QWidget;
    auto *layout = new QVBoxLayout(animation);
    auto *controls = new QHBoxLayout;
    auto *play = new QPushButton("Play / Pause");
    connect(play, &QPushButton::clicked, this, [this] { togglePlay(); });
    controls->addWidget(play);
    clips = new QComboBox;
    clips->setMinimumWidth(120);
    controls->addWidget(clips);
    connect(clips, &QComboBox::currentIndexChanged, this, [this](int i) {
        if (refreshing || i < 0)
            return;
        activeClip = static_cast<std::size_t>(i);
        playing = false;
        playback.stop();
        time = 0;
        refresh(false);
    });
    timeBox = new QDoubleSpinBox;
    timeBox->setDecimals(4);
    timeBox->setSuffix(" s");
    controls->addWidget(timeBox);
    connect(timeBox, &QDoubleSpinBox::valueChanged, this, [this](double t) {
        if (!refreshing)
            seek(t);
    });
    durationBox = new QDoubleSpinBox;
    durationBox->setRange(.05, 3600);
    durationBox->setDecimals(3);
    durationBox->setPrefix("Length ");
    controls->addWidget(durationBox);
    connect(durationBox, &QDoubleSpinBox::editingFinished, this, [this] {
        if (refreshing)
            return;
        double d = durationBox->value();
        change("Change duration",
               [=, this](Project &p) { resizeClip(p.clips[activeClip], d, scaleWithDuration->isChecked()); });
        timeline->setSelection({});
    });
    loop = new QCheckBox("Loop");
    controls->addWidget(loop);
    connect(loop, &QCheckBox::toggled, this, [this](bool b) {
        if (!refreshing)
            change("Toggle loop", [=, this](Project &p) { p.clips[activeClip].loop = b; });
    });
    fpsBox = new QComboBox;
    fpsBox->addItems({"24 FPS", "30 FPS", "60 FPS"});
    fpsBox->setCurrentIndex(1);
    controls->addWidget(fpsBox);
    connect(fpsBox, &QComboBox::currentIndexChanged, this, [this](int i) {
        timeline->fps = i == 0 ? 24 : i == 1 ? 30 : 60;
    });
    layout->addLayout(controls);
    controls = new QHBoxLayout;
    scaleWithDuration = new QCheckBox("Scale keys with length");
    scaleWithDuration->setChecked(true);
    controls->addWidget(scaleWithDuration);
    interpolation = new QComboBox;
    interpolation->addItems({"Linear", "Step", "Ease In", "Ease Out", "Ease In Out"});
    controls->addWidget(interpolation);
    auto *key = new QPushButton("◆ Insert key");
    controls->addWidget(key);
    connect(key, &QPushButton::clicked, this, [this] { insertKey(); });
    auto *del = new QPushButton("Delete key");
    controls->addWidget(del);
    connect(del, &QPushButton::clicked, this, [this] { removeKey(); });
    layout->addLayout(controls);
    timeline = new Timeline;
    timeline->project = &project;
    auto *scroll = new QScrollArea;
    scroll->setWidget(timeline);
    scroll->setWidgetResizable(true);
    layout->addWidget(scroll);
    timelineDock = dock("Animation timeline", "timeline", animation, Qt::BottomDockWidgetArea);
    timelineDock->setMinimumHeight(245);
    timeline->seek = [this](double t) {
        playing = false;
        playback.stop();
        seek(t);
    };
    timeline->selectPart = [this](int i) { select(i); };
}

void MainWindow::createMenus() {
    auto action = [this](QMenu *menu, QString label, QKeySequence shortcut, std::function<void()> fn) {
        auto *a = menu->addAction(label);
        a->setShortcut(shortcut);
        connect(a, &QAction::triggered, this, [fn = std::move(fn)] { fn(); });
        return a;
    };
    auto *file = menuBar()->addMenu("&File");
    action(file, "&New project…", QKeySequence::New, [this] { newProject(); });
    action(file, "&Open…", QKeySequence::Open, [this] { openProject(); });
    auto *recent = file->addMenu("Recent projects");
    connect(recent, &QMenu::aboutToShow, this, [this, recent] {
        recent->clear();
        for (const auto &path : QSettings().value("recent").toStringList()) {
            auto *a = recent->addAction(path);
            connect(a, &QAction::triggered, this, [this, path] { openProject(path); });
        }
    });
    action(file, "&Save", QKeySequence::Save, [this] { save(); });
    action(file, "Save &As…", QKeySequence::SaveAs, [this] { save(true); });
    file->addSeparator();
    action(file, "Export animation (.rbanim)…", QKeySequence("Ctrl+Shift+E"), [this] { exportClip(); });
    action(file, "Import model…", QKeySequence("Ctrl+I"), [this] { importModelFile(); });
    file->addSeparator();
    action(file, "Exit", QKeySequence::Quit, [this] { close(); });
    auto *edit = menuBar()->addMenu("&Edit");
    auto *undo = history.createUndoAction(this, "Undo");
    undo->setShortcut(QKeySequence::Undo);
    edit->addAction(undo);
    auto *redo = history.createRedoAction(this, "Redo");
    redo->setShortcut(QKeySequence::Redo);
    edit->addAction(redo);
    action(edit, "Add cube", QKeySequence("Ctrl+Shift+C"), [this] {
        change("Add cube", [](Project &p) {
            int n = 1;
            std::string name;
            do {
                name = "Cube" + std::to_string(n++);
            } while (std::any_of(p.parts.begin(), p.parts.end(),
                                 [&](const Part &a) { return a.name == name || a.joint == name; }));
            Part part;
            part.name = name;
            part.joint = name;
            part.parent = 0;
            part.c0.position = {3, 0, 0};
            p.parts.push_back(part);
            p.rigType = "Custom";
        });
    });
    action(edit, "Delete selected part and descendants", QKeySequence::Delete, [this] {
        if (typing())
            return;
        int s = viewport->selected;
        if (s <= 0)
            return;
        change("Delete part", [=](Project &p) {
            if (p.parts[s].locked)
                throw std::runtime_error("Part is locked");
            std::vector<bool> removed(p.parts.size());
            removed[s] = true;
            bool changed;
            do {
                changed = false;
                for (std::size_t i = 0; i < p.parts.size(); ++i)
                    if (p.parts[i].parent >= 0 && removed[p.parts[i].parent] && !removed[i]) {
                        removed[i] = true;
                        changed = true;
                    }
            } while (changed);
            std::vector<int> map(p.parts.size());
            std::vector<Part> keep;
            for (std::size_t i = 0; i < p.parts.size(); ++i) {
                if (removed[i]) {
                    for (auto &c : p.clips)
                        std::erase_if(c.tracks, [&](const Track &t) { return t.joint == p.parts[i].joint; });
                } else {
                    map[i] = static_cast<int>(keep.size());
                    keep.push_back(p.parts[i]);
                }
            }
            for (auto &part : keep)
                if (part.parent >= 0)
                    part.parent = map[part.parent];
            p.parts = std::move(keep);
            p.rigType = "Custom";
        });
    });
    auto *animation = menuBar()->addMenu("&Animation");
    action(animation, "New clip…", {}, [this] {
        bool ok = false;
        auto name = QInputDialog::getText(this, "New animation", "Name", QLineEdit::Normal, "Animation", &ok);
        if (ok && !name.trimmed().isEmpty())
            change("Create animation", [=](Project &p) {
                Clip c;
                c.name = name.trimmed().toStdString();
                p.clips.push_back(c);
            });
    });
    action(animation, "Rename clip…", {}, [this] {
        bool ok = false;
        auto name = QInputDialog::getText(this, "Rename animation", "Name", QLineEdit::Normal,
                                          QString::fromStdString(project.clips[activeClip].name), &ok);
        if (ok)
            change("Rename animation",
                   [=, this](Project &p) { p.clips[activeClip].name = name.trimmed().toStdString(); });
    });
    action(animation, "Duplicate clip", {}, [this] {
        change("Duplicate animation", [this](Project &p) {
            auto copy = p.clips[activeClip];
            copy.name += " Copy";
            p.clips.push_back(copy);
        });
    });
    action(animation, "Delete clip", {}, [this] {
        if (project.clips.size() == 1)
            return;
        change("Delete animation", [this](Project &p) {
            p.clips.erase(p.clips.begin() + static_cast<std::ptrdiff_t>(activeClip));
        });
    });
    action(animation, "Insert key", QKeySequence("K"), [this] {
        if (!typing())
            insertKey();
    });
    action(animation, "Delete key", QKeySequence("Shift+K"), [this] {
        if (!typing())
            removeKey();
    });
    action(animation, "Play / Pause", QKeySequence("Space"), [this] {
        if (!typing())
            togglePlay();
    });
    auto *view = menuBar()->addMenu("&View");
    for (auto *d : findChildren<QDockWidget *>())
        view->addAction(d->toggleViewAction());
    auto *ortho = action(view, "Orthographic", QKeySequence("5"), [] {});
    ortho->setCheckable(true);
    connect(ortho, &QAction::toggled, this, [this](bool b) { viewport->setOrthographic(b); });
    auto *grid = action(view, "Grid", {}, [] {});
    grid->setCheckable(true);
    grid->setChecked(true);
    connect(grid, &QAction::toggled, this, [this](bool b) {
        viewport->showGrid = b;
        viewport->requestUpdate();
    });
    auto *antialias = action(view, "4x anti-aliasing (next launch)", {}, [] {});
    antialias->setCheckable(true);
    antialias->setEnabled(viewport->supportedSampleCounts().contains(4));
    antialias->setChecked(viewport->supportedSampleCounts().contains(4) &&
                          QSettings().value("viewportMsaa", true).toBool());
    connect(antialias, &QAction::toggled, this, [this](bool enabled) {
        QSettings().setValue("viewportMsaa", enabled);
        statusBar()->showMessage("Viewport anti-aliasing setting saved; takes effect next launch.", 5000);
    });
    action(view, "Focus selection", QKeySequence("F"), [this] {
        if (!typing())
            viewport->focusSelected();
    });
    action(view, "Front", QKeySequence("1"), [this] {
        if (!typing())
            viewport->setView(0);
    });
    action(view, "Right", QKeySequence("3"), [this] {
        if (!typing())
            viewport->setView(1);
    });
    action(view, "Top", QKeySequence("7"), [this] {
        if (!typing())
            viewport->setView(2);
    });
    action(view, "Save workspace layout", {}, [this] { QSettings().setValue("customLayout", saveState(1)); });
    action(view, "Restore workspace layout", {},
           [this] { restoreState(QSettings().value("customLayout").toByteArray(), 1); });
    auto *help = menuBar()->addMenu("&Help");
    action(help, "Check for updates…", {}, [this] { showUpdater(); });
    action(help, "About RobBench", {}, [this] {
        QMessageBox::about(this, "RobBench",
                           "RobBench 0.2.0 — native C++20 / Qt 6 / Vulkan 1.3\nR6/R15 animation authoring "
                           "foundation.\nIndependent project; not affiliated with Roblox or Blockbench.\nSee "
                           "docs/STATUS.md for verified capabilities and limitations.");
    });
    auto *toolbar = addToolBar("Project & workspace");
    toolbar->setObjectName("mainToolbar");
    toolbar->addAction(file->actions()[0]);
    toolbar->addAction(file->actions()[1]);
    toolbar->addAction(file->actions()[3]);
    toolbar->addSeparator();
    auto *workspace = new QComboBox;
    workspace->addItems({"Animate", "Edit", "Display"});
    toolbar->addWidget(workspace);
    connect(workspace, &QComboBox::currentTextChanged, this,
            [this](const QString &s) { timelineDock->setVisible(s == "Animate"); });
    toolbar->addSeparator();
    auto *note = new QLabel("  RMB Orbit   •   MMB Pan   •   Wheel Zoom   •   K Keyframe  ");
    toolbar->addWidget(note);
    statusBar()->showMessage("Ready — select a body part, rotate it in Properties, and scrub the timeline.");
}
void MainWindow::refresh(bool tree) {
    refreshing = true;
    viewport->cancelDrag();
    activeClip = std::min(activeClip, project.clips.size() - 1);
    time = std::clamp(time, 0., project.clips[activeClip].duration);
    viewport->pose = evaluatePose(project, activeClip, time);
    if (viewport->selected >= static_cast<int>(project.parts.size()))
        viewport->selected = -1;
    if (tree) {
        outliner->clear();
        std::vector<QTreeWidgetItem *> items;
        for (std::size_t i = 0; i < project.parts.size(); ++i) {
            auto *item = new QTreeWidgetItem;
            item->setText(0, QString::fromStdString(project.parts[i].name));
            item->setData(0, Qt::UserRole, static_cast<int>(i));
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(1, project.parts[i].visible ? Qt::Checked : Qt::Unchecked);
            item->setCheckState(2, project.parts[i].locked ? Qt::Checked : Qt::Unchecked);
            items.push_back(item);
        }
        for (std::size_t i = 0; i < items.size(); ++i) {
            int parent = project.parts[i].parent;
            if (parent < 0)
                outliner->addTopLevelItem(items[i]);
            else
                items[parent]->addChild(items[i]);
        }
        outliner->expandAll();
        if (viewport->selected >= 0)
            outliner->setCurrentItem(items[viewport->selected]);
    }
    bool clipListChanged = clips->count() != static_cast<int>(project.clips.size());
    if (!clipListChanged)
        for (int i = 0; i < clips->count(); ++i)
            if (clips->itemText(i) != QString::fromStdString(project.clips[i].name)) {
                clipListChanged = true;
                break;
            }
    if (clipListChanged) {
        clips->clear();
        for (const auto &c : project.clips)
            clips->addItem(QString::fromStdString(c.name));
    }
    clips->setCurrentIndex(static_cast<int>(activeClip));
    const auto &c = project.clips[activeClip];
    timeBox->setRange(0, c.duration);
    timeBox->setValue(time);
    durationBox->setValue(c.duration);
    loop->setChecked(c.loop);
    int s = viewport->selected;
    bool enabled = s >= 0 && !project.parts[s].locked;
    selectedLabel->setText(
        s >= 0 ? QString::fromStdString(project.parts[s].name + "\nMotor6D: " + project.parts[s].joint)
               : "Select a body part");
    for (auto *spin : transforms)
        spin->setEnabled(enabled);
    for (auto *spin : dimensions)
        spin->setEnabled(enabled);
    if (s >= 0) {
        const auto &pose = viewport->pose[s];
        auto euler = glm::degrees(glm::eulerAngles(pose.rotation));
        for (int i = 0; i < 3; ++i) {
            transforms[i]->setValue(pose.position[i]);
            transforms[i + 3]->setValue(euler[i]);
            dimensions[i]->setValue(project.parts[s].size[i]);
        }
    }
    timeline->clip = activeClip;
    timeline->time = time;
    timeline->selected = s;
    timeline->sync();
    viewport->requestUpdate();
    refreshing = false;
}
void MainWindow::select(int i) {
    viewport->selected = i;
    viewport->hoveredAxis = -1;
    refresh(false);
    if (i < 0) {
        outliner->clearSelection();
        return;
    }
    if (i >= 0) {
        refreshing = true;
        QTreeWidgetItemIterator iter(outliner);
        while (*iter) {
            if ((*iter)->data(0, Qt::UserRole).toInt() == i) {
                outliner->setCurrentItem(*iter);
                break;
            }
            ++iter;
        }
        refreshing = false;
    }
}
void MainWindow::seek(double t) {
    time = t;
    refresh(false);
}
void MainWindow::change(const QString &name, const std::function<void(Project &)> &edit) {
    playing = false;
    playback.stop();
    try {
        Project next = project;
        edit(next);
        next.validate();
        if (projectJson(next) == projectJson(project)) {
            refresh();
            return;
        }
        history.push(new EditCommand(project, std::move(next), name, [this] { refresh(); }));
    } catch (const std::exception &e) {
        showError(e);
        refresh();
    }
}
void MainWindow::editTransform() {
    if (refreshing || viewport->selected < 0 || playing)
        return;
    int i = viewport->selected;
    if (project.parts[i].locked)
        return;
    Transform pose;
    for (int n = 0; n < 3; ++n)
        pose.position[n] = static_cast<float>(transforms[n]->value());
    pose.rotation = glm::normalize(glm::quat(
        glm::radians(glm::vec3(transforms[3]->value(), transforms[4]->value(), transforms[5]->value()))));
    const auto &current = viewport->pose[i];
    if (glm::length(pose.position - current.position) < 1e-5f &&
        std::min(glm::length(pose.rotation - current.rotation),
                 glm::length(pose.rotation + current.rotation)) < 1e-6f)
        return;
    double t = time;
    auto joint = project.parts[i].joint;
    auto mode = interpolation->currentIndex();
    Interpolation easing = mode == 0   ? Interpolation::Linear
                           : mode == 1 ? Interpolation::Step
                                       : static_cast<Interpolation>(mode);
    change("Edit joint keyframe",
           [=, this](Project &p) { setKey(p.clips[activeClip], joint, {t, pose, easing}); });
}
void MainWindow::insertKey() {
    if (viewport->selected < 0)
        return;
    int i = viewport->selected;
    if (project.parts[i].locked)
        return;
    auto pose = viewport->pose[i];
    auto joint = project.parts[i].joint;
    double t = time;
    int mode = interpolation->currentIndex();
    Interpolation easing = mode == 0   ? Interpolation::Linear
                           : mode == 1 ? Interpolation::Step
                                       : static_cast<Interpolation>(mode);
    change("Insert keyframe",
           [=, this](Project &p) { setKey(p.clips[activeClip], joint, {t, pose, easing}); });
}
void MainWindow::removeKey() {
    if (!timeline->selection().empty()) {
        const auto keys = timeline->selection();
        change("Delete selected keyframes", [=, this](Project &p) { deleteKeys(p.clips[activeClip], keys); });
        timeline->setSelection({});
        return;
    }
    if (viewport->selected < 0)
        return;
    auto joint = project.parts[viewport->selected].joint;
    double t = time;
    change("Delete keyframe", [=, this](Project &p) { deleteKey(p.clips[activeClip], joint, t); });
}
void MainWindow::togglePlay() {
    playing = !playing;
    if (playing) {
        if (time >= project.clips[activeClip].duration)
            time = 0;
        clock.start();
        playback.start();
    } else
        playback.stop();
}
void MainWindow::showError(const std::exception &e) {
    spdlog::error("{}", e.what());
    QMessageBox::critical(this, "RobBench", QString::fromUtf8(e.what()));
}
bool MainWindow::maybeSave() {
    if (history.isClean())
        return true;
    auto result = QMessageBox::warning(this, "Unsaved project", "Save changes before continuing?",
                                       QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (result == QMessageBox::Cancel)
        return false;
    if (result == QMessageBox::Save)
        return save();
    return true;
}
void MainWindow::newProject() {
    if (!maybeSave())
        return;
    bool ok = false;
    auto type = QInputDialog::getItem(this, "New project", "Rig", {"R15", "R6"}, 0, false, &ok);
    if (!ok)
        return;
    playing = false;
    playback.stop();
    project = makeRig(type.toStdString());
    activeClip = 0;
    time = 0;
    filename.clear();
    history.clear();
    viewport->selected = -1;
    setWindowTitle("RobBench — Untitled[*]");
    refresh();
}
void MainWindow::openProject(const QString &given) {
    if (!maybeSave())
        return;
    auto path = given.isEmpty()
                    ? QFileDialog::getOpenFileName(this, "Open project", {}, "RobBench (*.robbench *.rbanim)")
                    : given;
    if (path.isEmpty())
        return;
    try {
        auto loaded = loadProject(pathOf(path));
        playing = false;
        playback.stop();
        project = std::move(loaded);
        activeClip = 0;
        time = 0;
        viewport->selected = -1;
        history.clear();
        filename =
            (path == recoveryPath() || path.endsWith(".rbanim", Qt::CaseInsensitive)) ? QString() : path;
        setWindowTitle("RobBench — " +
                       (filename.isEmpty() ? "Recovered project" : QFileInfo(path).fileName()) + "[*]");
        remember(path);
        refresh();
        if (filename.isEmpty()) {
            auto copy = project;
            history.push(new EditCommand(project, std::move(copy), "Recover project", [this] { refresh(); }));
        }
    } catch (const std::exception &e) {
        showError(e);
    }
}
bool MainWindow::save(bool saveAs) {
    auto path = filename;
    if (saveAs || path.isEmpty())
        path = QFileDialog::getSaveFileName(this, "Save project", path, "RobBench (*.robbench)");
    if (path.isEmpty())
        return false;
    if (!path.endsWith(".robbench", Qt::CaseInsensitive))
        path += ".robbench";
    try {
        saveProject(project, pathOf(path));
        filename = path;
        history.setClean();
        setWindowTitle("RobBench — " + QFileInfo(path).fileName() + "[*]");
        remember(path);
        QFile::remove(recoveryPath());
        statusBar()->showMessage("Project saved", 3000);
        return true;
    } catch (const std::exception &e) {
        showError(e);
        return false;
    }
}
void MainWindow::exportClip() {
    auto path = QFileDialog::getSaveFileName(this, "Export baked Roblox animation", {},
                                             "RobBench animation (*.rbanim)");
    if (path.isEmpty())
        return;
    if (!path.endsWith(".rbanim", Qt::CaseInsensitive))
        path += ".rbanim";
    try {
        exportAnimation(project, activeClip, pathOf(path), timeline->fps);
        statusBar()->showMessage("Animation exported. Use the Studio companion plugin to import it.", 6000);
    } catch (const std::exception &e) {
        showError(e);
    }
}
void MainWindow::remember(const QString &path) {
    QSettings s;
    auto recent = s.value("recent").toStringList();
    recent.removeAll(path);
    recent.prepend(path);
    while (recent.size() > 10)
        recent.removeLast();
    s.setValue("recent", recent);
}
QString MainWindow::recoveryPath() const {
    auto dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dir);
    return dir + "/recovery.robbench";
}
void MainWindow::closeEvent(QCloseEvent *e) {
    if (!maybeSave()) {
        e->ignore();
        return;
    }
    QSettings s;
    s.setValue("geometry", saveGeometry());
    s.setValue("layout", saveState(1));
    QFile::remove(recoveryPath());
    e->accept();
}
} // namespace rb

namespace rb {
void MainWindow::importModelFile() {
    auto filename =
        QFileDialog::getOpenFileName(this, "Import static model", {}, "Models (*.obj *.fbx *.gltf *.glb)");
    if (filename.isEmpty())
        return;
    bool accepted = false;
    double scale =
        QInputDialog::getDouble(this, "Model scale", "Source units to studs", 1, .0001, 10000, 4, &accepted);
    if (!accepted)
        return;
    std::size_t count = 0;
    change("Import model",
           [&, this](Project &p) { count = importModel(p, pathOf(filename), static_cast<float>(scale)); });
    if (count)
        statusBar()->showMessage(
            QString("Imported %1 mesh objects. Texture images and skinned animation are not imported.")
                .arg(count),
            10000);
}
} // namespace rb
