#pragma once
#include "core/Project.h"
#include <QWidget>
#include <functional>
namespace rb {
class Timeline final : public QWidget {
  public:
    Project *project{};
    std::size_t clip{0};
    double time{0};
    int selected{-1};
    int fps{30};
    std::function<void(double)> seek;
    std::function<void(int)> selectPart;
    std::function<void(std::string, double, double)> moveKey;
    explicit Timeline(QWidget *parent = nullptr);
    void sync();

  protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;

  private:
    double pixelsPerSecond{150};
    bool dragging{false};
    std::string draggedJoint;
    double draggedTime{0}, dropTime{0};
    double at(int x) const;
};
} // namespace rb
