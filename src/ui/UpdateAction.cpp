#include "ui/MainWindow.h"
#include "updates/UpdateDialog.h"
namespace rb {
void MainWindow::showUpdater() {
    UpdateDialog dialog(this, [this] { return maybeSave(); });
    dialog.exec();
}
} // namespace rb
