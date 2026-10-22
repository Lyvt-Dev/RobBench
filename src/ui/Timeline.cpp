#include "ui/Timeline.h"
#include <QKeyEvent>
#include <QKeySequence>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <optional>
namespace rb {
Timeline::Timeline(QWidget *p) : QWidget(p) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumHeight(180);
    setToolTip("Ctrl/Shift+click adds keys • drag empty space to box-select • drag selected keys together • "
               "Ctrl+A/C/V • Delete • Esc");
}
const Clip *Timeline::clipData() const {
    return project && clip < project->clips.size() ? &project->clips[clip] : nullptr;
}
void Timeline::notify() {
    update();
    if (selectionChanged)
        selectionChanged();
}
void Timeline::setSelection(KeySelection value) {
    selection_ = std::move(value);
    selectionClip = clip;
    notify();
}
void Timeline::selectAllKeys() {
    KeySelection all;
    if (auto *c = clipData())
        for (auto &t : c->tracks)
            for (auto &k : t.keys)
                all.insert({t.joint, k.time});
    setSelection(std::move(all));
}
void Timeline::sync() {
    if (auto *c = clipData()) {
        setMinimumSize(static_cast<int>(std::min(16000000., 210 + c->duration * pixelsPerSecond)),
                       40 + static_cast<int>(project->parts.size()) * 24);
        if (selectionClip != clip) {
            selection_.clear();
            selectionClip = clip;
        }
        KeySelection valid;
        for (const auto &t : c->tracks)
            for (const auto &k : t.keys)
                if (selection_.contains({t.joint, k.time}))
                    valid.insert({t.joint, k.time});
        selection_ = std::move(valid);
    }
    update();
}
double Timeline::at(int x) const {
    auto *c = clipData();
    return c ? std::clamp(std::round((x - 190) / pixelsPerSecond * fps) / fps, 0., c->duration) : 0;
}
bool Timeline::event(QEvent *e) {
    if (e->type() == QEvent::ShortcutOverride) {
        auto *k = static_cast<QKeyEvent *>(e);
        if (k->key() == Qt::Key_Delete || k->key() == Qt::Key_Backspace ||
            k->matches(QKeySequence::SelectAll) || k->matches(QKeySequence::Copy) ||
            k->matches(QKeySequence::Paste)) {
            e->accept();
            return true;
        }
    }
    return QWidget::event(e);
}
void Timeline::paintEvent(QPaintEvent *event) {
    QPainter p(this);
    p.fillRect(event->rect(), QColor("#20232a"));
    auto *c = clipData();
    if (!c)
        return;
    double tick = std::max(1., std::ceil(60 / pixelsPerSecond * fps)) / fps;
    const double first =
        std::max(0., std::floor((event->rect().left() - 190) / pixelsPerSecond / tick) * tick);
    for (double t = first; t <= c->duration + 1e-8 && 190 + t * pixelsPerSecond <= event->rect().right() + 60;
         t += tick) {
        int x = 190 + static_cast<int>(t * pixelsPerSecond);
        p.setPen(QColor("#30343e"));
        p.drawLine(x, 28, x, height());
        p.setPen(QColor("#9297a6"));
        p.drawText(x + 3, 18, QString::number(t, 'f', tick < .1 ? 2 : 1));
    }
    p.fillRect(0, 0, 188, height(), QColor("#292c34"));
    p.setPen(QColor("#b7bdca"));
    p.drawText(10, 19, QString("Joints • %1 selected").arg(selection_.size()));
    for (std::size_t i = 0; i < project->parts.size(); ++i) {
        int y = 30 + static_cast<int>(i) * 24;
        if (y > event->rect().bottom() || y + 24 < event->rect().top())
            continue;
        if (static_cast<int>(i) == selected)
            p.fillRect(0, y, width(), 24, QColor(122, 92, 170, 50));
        p.setPen(QColor("#c6cad4"));
        p.drawText(10, y + 17, QString::fromStdString(project->parts[i].joint));
        p.setPen(QColor("#333741"));
        p.drawLine(0, y + 23, width(), y + 23);
        for (const auto &t : c->tracks)
            if (t.joint == project->parts[i].joint)
                for (const auto &k : t.keys) {
                    bool chosen = selection_.contains({t.joint, k.time});
                    double timestamp = k.time + (chosen && dragging ? delta : 0);
                    int x = 190 + static_cast<int>(timestamp * pixelsPerSecond);
                    if (x < 185)
                        continue;
                    p.setPen(chosen ? QPen(QColor("#fff0c4"), 1) : QPen(Qt::NoPen));
                    p.setBrush(chosen ? QColor("#eebc69") : QColor("#aa89e6"));
                    QPolygon d;
                    d << QPoint(x, y + 5) << QPoint(x + 6, y + 12) << QPoint(x, y + 19)
                      << QPoint(x - 6, y + 12);
                    p.drawPolygon(d);
                }
    }
    p.setPen(QPen(QColor("#d2b7ff"), 2));
    int x = 190 + static_cast<int>(time * pixelsPerSecond);
    p.drawLine(x, 25, x, height());
    if (marquee) {
        p.setPen(QColor("#b595f2"));
        p.setBrush(QColor(150, 110, 230, 45));
        p.drawRect(QRect(pressPoint, currentPoint).normalized());
    }
}
void Timeline::mousePressEvent(QMouseEvent *e) {
    auto *c = clipData();
    if (!c || e->button() != Qt::LeftButton)
        return;
    setFocus();
    pressPoint = currentPoint = e->position().toPoint();
    int row = (pressPoint.y() - 30) / 24;
    const bool additive =
        e->modifiers().testFlag(Qt::ControlModifier) || e->modifiers().testFlag(Qt::ShiftModifier);
    if (pressPoint.y() >= 30 && row >= 0 && row < static_cast<int>(project->parts.size())) {
        std::optional<KeyRef> hit;
        if (pressPoint.x() >= 185)
            for (const auto &t : c->tracks)
                if (t.joint == project->parts[row].joint)
                    for (const auto &k : t.keys)
                        if (std::abs(190 + k.time * pixelsPerSecond - pressPoint.x()) < 9)
                            hit = KeyRef{t.joint, k.time};
        if (selectPart)
            selectPart(row);
        if (hit) {
            if (additive) {
                if (selection_.contains(*hit))
                    selection_.erase(*hit);
                else
                    selection_.insert(*hit);
                notify();
                return;
            }
            if (!selection_.contains(*hit))
                selection_ = {*hit};
            dragging = true;
            delta = 0;
            if (seek)
                seek(hit->time);
            notify();
            return;
        }
    }
    if (pressPoint.x() >= 190 && pressPoint.y() >= 28) {
        marquee = true;
        beforeMarquee = additive ? selection_ : KeySelection{};
        if (!additive)
            selection_.clear();
        notify();
    } else if (pressPoint.x() >= 190 && seek)
        seek(at(pressPoint.x()));
}
void Timeline::mouseMoveEvent(QMouseEvent *e) {
    currentPoint = e->position().toPoint();
    auto *c = clipData();
    if (!c)
        return;
    if (dragging) {
        delta = std::round((currentPoint.x() - pressPoint.x()) / pixelsPerSecond * fps) / fps;
        double low = 0, high = c->duration;
        for (const auto &k : selection_) {
            high = std::min(high, c->duration - k.time);
        }
        low = selection_.empty()
                  ? 0
                  : -std::min_element(selection_.begin(), selection_.end(), [](const auto &a, const auto &b) {
                         return a.time < b.time;
                     })->time;
        delta = std::clamp(delta, low, high);
        update();
    } else if (marquee) {
        auto rect = QRect(pressPoint, currentPoint).normalized();
        selection_ = beforeMarquee;
        for (std::size_t i = 0; i < project->parts.size(); ++i)
            for (const auto &t : c->tracks)
                if (t.joint == project->parts[i].joint)
                    for (const auto &k : t.keys)
                        if (rect.contains(QPoint(190 + static_cast<int>(k.time * pixelsPerSecond),
                                                 42 + static_cast<int>(i) * 24)))
                            selection_.insert({t.joint, k.time});
        notify();
    } else if ((e->buttons() & Qt::LeftButton) && currentPoint.x() >= 190 && seek)
        seek(at(currentPoint.x()));
}
void Timeline::mouseReleaseEvent(QMouseEvent *e) {
    if (e->button() != Qt::LeftButton)
        return;
    if (dragging) {
        dragging = false;
        if (std::abs(delta) > 1e-10 && moveKeys)
            moveKeys(selection_, delta);
        delta = 0;
        update();
    }
    if (marquee) {
        marquee = false;
        if ((currentPoint - pressPoint).manhattanLength() < 4 && seek)
            seek(at(currentPoint.x()));
        notify();
    }
}
void Timeline::keyPressEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_Escape) {
        dragging = marquee = false;
        delta = 0;
        selection_.clear();
        notify();
        return;
    }
    if (e->matches(QKeySequence::SelectAll)) {
        selectAllKeys();
        return;
    }
    if (e->matches(QKeySequence::Copy)) {
        if (copySelection)
            copySelection();
        return;
    }
    if (e->matches(QKeySequence::Paste)) {
        if (pasteSelection)
            pasteSelection();
        return;
    }
    if (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) {
        if (deleteSelection)
            deleteSelection();
        return;
    }
    QWidget::keyPressEvent(e);
}
void Timeline::wheelEvent(QWheelEvent *e) {
    if (e->modifiers() & Qt::ControlModifier) {
        pixelsPerSecond = std::clamp(pixelsPerSecond * std::exp(e->angleDelta().y() * .001), 20., 1000.);
        sync();
        e->accept();
    } else
        e->ignore();
}
} // namespace rb
