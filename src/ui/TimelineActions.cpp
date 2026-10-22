#include "ui/MainWindow.h"
#include "ui/Timeline.h"
#include <QInputDialog>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <algorithm>
namespace rb {
void MainWindow::setupTimelineActions() {
    timeline->moveKeys = [this](const KeySelection &selected, double delta) {
        auto refs = selected;
        KeySelection moved;
        bool ok = false;
        change("Move selected keyframes", [&, this](Project &p) {
            moved = transformKeyTimes(p.clips[activeClip], refs, delta);
            ok = true;
        });
        if (ok)
            timeline->setSelection(std::move(moved));
    };
    timeline->deleteSelection = [this] { removeKey(); };
    timeline->copySelection = [this] { copySelectedKeys(); };
    timeline->pasteSelection = [this] { pasteSelectedKeys(); };
    timeline->selectionChanged = [this] {
        statusBar()->showMessage(
            QString("%1 keyframes selected • Ctrl/Shift-click or box-select • drag together")
                .arg(timeline->selection().size()),
            3000);
    };
    auto *menu = new QMenu("&Keyframes", this);
    menuBar()->insertMenu(menuBar()->actions().at(3), menu);
    auto add = [this, menu](const QString &label, auto callback) {
        auto *a = menu->addAction(label);
        connect(a, &QAction::triggered, this, callback);
    };
    add("Select all", [this] { timeline->selectAllKeys(); });
    add("Copy selected", [this] { copySelectedKeys(); });
    add("Paste at cursor", [this] { pasteSelectedKeys(); });
    add("Delete selected", [this] { removeKey(); });
    add("Scale selected timing…", [this] { scaleSelectedKeys(); });
}
void MainWindow::copySelectedKeys() {
    try {
        keyClipboard = copyKeys(project.clips[activeClip], timeline->selection());
        statusBar()->showMessage(QString("Copied %1 keyframes").arg(keyClipboard.size()), 3000);
    } catch (const std::exception &e) {
        showError(e);
    }
}
void MainWindow::pasteSelectedKeys() {
    if (keyClipboard.empty())
        return;
    KeySelection pasted;
    bool ok = false;
    change("Paste keyframes", [&, this](Project &p) {
        pasted = pasteKeys(p.clips[activeClip], keyClipboard, time);
        ok = true;
    });
    if (ok)
        timeline->setSelection(std::move(pasted));
}
void MainWindow::scaleSelectedKeys() {
    const auto selected = timeline->selection();
    if (selected.empty())
        return;
    bool accepted = false;
    double factor = QInputDialog::getDouble(
        this, "Scale keyframe timing", "Factor (anchor: earliest selected key)", 1, .01, 100, 3, &accepted);
    if (!accepted)
        return;
    double pivot = selected.begin()->time;
    for (const auto &k : selected)
        pivot = std::min(pivot, k.time);
    KeySelection mapped;
    bool ok = false;
    change("Scale selected timing", [&, this](Project &p) {
        mapped = transformKeyTimes(p.clips[activeClip], selected, 0, factor, pivot);
        ok = true;
    });
    if (ok)
        timeline->setSelection(std::move(mapped));
}
} // namespace rb
