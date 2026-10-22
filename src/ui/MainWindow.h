#pragma once
#include "animation/KeyEditing.h"
#include "core/Project.h"
#include <QElapsedTimer>
#include <QMainWindow>
#include <QTimer>
#include <QUndoStack>
#include <functional>
class QVulkanInstance;
class QTreeWidget;
class QDoubleSpinBox;
class QComboBox;
class QCheckBox;
class QLabel;
class QDockWidget;
namespace rb {
class Viewport;
class Timeline;
class MainWindow final : public QMainWindow {
  public:
    explicit MainWindow(QVulkanInstance *instance);
    ~MainWindow() override;
    void startSmokeTest(const QString &directory, const QString &rigType = "R15");

  protected:
    void closeEvent(QCloseEvent *) override;

  private:
    Project project;
    Viewport *viewport{};
    Timeline *timeline{};
    QTreeWidget *outliner{};
    QComboBox *clips{};
    QComboBox *interpolation{};
    QComboBox *fpsBox{};
    QDoubleSpinBox *timeBox{};
    QDoubleSpinBox *durationBox{};
    QCheckBox *loop{};
    QCheckBox *scaleWithDuration{};
    std::vector<CopiedKey> keyClipboard;
    void setupTimelineActions();
    void copySelectedKeys();
    void pasteSelectedKeys();
    void scaleSelectedKeys();
    QLabel *selectedLabel{};
    QDoubleSpinBox *transforms[6]{};
    QDoubleSpinBox *dimensions[3]{};
    QDockWidget *timelineDock{};
    QUndoStack history;
    QTimer playback, autosave;
    QElapsedTimer clock;
    std::size_t activeClip{0};
    double time{0};
    bool refreshing{false}, playing{false};
    QString filename;
    void refresh(bool tree = true);
    void select(int index);
    void seek(double value);
    void editTransform();
    void change(const QString &name, const std::function<void(Project &)> &edit);
    void insertKey();
    void removeKey();
    void newProject();
    void openProject(const QString &path = {});
    bool save(bool saveAs = false);
    bool maybeSave();
    void exportClip();
    void importModelFile();
    void showUpdater();
    void togglePlay();
    void showError(const std::exception &);
    void createMenus();
    void createPanels();
    void createTransformTools();
    void runGizmoSmokeTest();
    void remember(const QString &path);
    QString recoveryPath() const;
};
} // namespace rb
