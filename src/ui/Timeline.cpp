#include "ui/Timeline.h"
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
namespace rb {
Timeline::Timeline(QWidget *p) : QWidget(p) {
    setMouseTracking(true);
    setMinimumHeight(180);
    setToolTip("Click to scrub • drag a key to move it • Ctrl+wheel to zoom");
}
void Timeline::sync() {
    if (project) {
        setMinimumSize(210 + static_cast<int>(project->clips[clip].duration * pixelsPerSecond),
                       40 + static_cast<int>(project->parts.size()) * 24);
    }
    update();
}
double Timeline::at(int x) const {
    if (!project)
        return 0;
    return std::clamp(std::round((x - 190) / pixelsPerSecond * fps) / fps, 0., project->clips[clip].duration);
}
void Timeline::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.fillRect(rect(), QColor("#22252c"));
    if (!project)
        return;
    const auto &c = project->clips[clip];
    p.setPen(QColor("#6d7180"));
    double tick = std::max(1., std::ceil(60 / pixelsPerSecond * fps)) / fps;
    for (double t = 0; t <= c.duration + 1e-8; t += tick) {
        int x = 190 + static_cast<int>(t * pixelsPerSecond);
        p.setPen(QColor("#30343e"));
        p.drawLine(x, 28, x, height());
        p.setPen(QColor("#9297a6"));
        p.drawText(x + 3, 18, QString::number(t, 'f', tick < .1 ? 2 : 1));
    }
    p.fillRect(0, 0, 188, height(), QColor("#292c34"));
    p.setPen(QColor("#b7bdca"));
    p.drawText(10, 19, "Joint / Motor6D");
    for (std::size_t i = 0; i < project->parts.size(); ++i) {
        int y = 30 + static_cast<int>(i) * 24;
        if (static_cast<int>(i) == selected)
            p.fillRect(0, y, width(), 24, QColor(122, 92, 170, 50));
        p.setPen(QColor("#c6cad4"));
        p.drawText(10, y + 17, QString::fromStdString(project->parts[i].joint));
        p.setPen(QColor("#333741"));
        p.drawLine(0, y + 23, width(), y + 23);
        for (const auto &t : c.tracks)
            if (t.joint == project->parts[i].joint)
                for (const auto &k : t.keys) {
                    int x = 190 + static_cast<int>(k.time * pixelsPerSecond);
                    p.setPen(Qt::NoPen);
                    p.setBrush(std::abs(k.time - time) < 1e-7 ? QColor("#f4cf84") : QColor("#aa89e6"));
                    QPolygon d;
                    d << QPoint(x, y + 6) << QPoint(x + 5, y + 12) << QPoint(x, y + 18)
                      << QPoint(x - 5, y + 12);
                    p.drawPolygon(d);
                }
    }
    if (dragging) {
        p.setPen(QColor("#f4cf84"));
        int x = 190 + static_cast<int>(dropTime * pixelsPerSecond);
        p.drawLine(x, 28, x, height());
    }
    p.setPen(QPen(QColor("#d2b7ff"), 2));
    int x = 190 + static_cast<int>(time * pixelsPerSecond);
    p.drawLine(x, 25, x, height());
}
void Timeline::mousePressEvent(QMouseEvent *e) {
    if (!project || e->button() != Qt::LeftButton)
        return;
    int row = (e->position().toPoint().y() - 30) / 24;
    if (e->position().y() >= 30 && row >= 0 && row < static_cast<int>(project->parts.size())) {
        if (selectPart)
            selectPart(row);
        if (e->position().x() > 185)
            for (const auto &t : project->clips[clip].tracks)
                if (t.joint == project->parts[row].joint)
                    for (const auto &k : t.keys)
                        if (std::abs(190 + k.time * pixelsPerSecond - e->position().x()) < 8) {
                            dragging = true;
                            draggedJoint = t.joint;
                            draggedTime = k.time;
                            dropTime = k.time;
                            if (seek)
                                seek(k.time);
                            return;
                        }
    }
    if (e->position().x() >= 190 && seek)
        seek(at(e->position().toPoint().x()));
}
void Timeline::mouseMoveEvent(QMouseEvent *e) {
    if (dragging) {
        dropTime = at(e->position().toPoint().x());
        update();
    } else if ((e->buttons() & Qt::LeftButton) && e->position().x() >= 190 && seek)
        seek(at(e->position().toPoint().x()));
}
void Timeline::mouseReleaseEvent(QMouseEvent *) {
    if (dragging) {
        dragging = false;
        if (moveKey && dropTime != draggedTime)
            moveKey(draggedJoint, draggedTime, dropTime);
        update();
    }
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
