#pragma once
#include "animation/KeyEditing.h"
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
    std::function<void(const KeySelection &, double)> moveKeys;
    std::function<void()> selectionChanged;
    std::function<void()> deleteSelection, copySelection, pasteSelection;
    explicit Timeline(QWidget *parent = nullptr);
    void sync();
    const KeySelection &selection() const {
        return selection_;
    }
    void setSelection(KeySelection value);
    void selectAllKeys();

  protected:
    bool event(QEvent *) override;
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void keyPressEvent(QKeyEvent *) override;

  private:
    double pixelsPerSecond{150};
    bool dragging{false}, marquee{false};
    double delta{0};
    QPoint pressPoint, currentPoint;
    KeySelection selection_, beforeMarquee;
    std::size_t selectionClip{0};
    const Clip *clipData() const;
    double at(int x) const;
    void notify();
};
} // namespace rb
