#include "ui/MainWindow.h"
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QPalette>
#include <QSettings>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QVulkanInstance>
#include <atomic>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <vulkan/vulkan.h>
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("RobBench");
    QApplication::setOrganizationName("RobBench");
    QApplication::setApplicationVersion("0.1.1");
    auto arguments = app.arguments();
    const bool smoke = arguments.contains("--smoke-test");
    if (smoke) {
        QStandardPaths::setTestModeEnabled(true);
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           QDir::tempPath() + "/robbench-smoke-settings");
    }
    try {
        const auto logs = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        QDir().mkpath(logs);
        auto file = spdlog::basic_logger_mt("file", (logs + "/RobBench.log").toStdString());
        spdlog::default_logger()->sinks().push_back(file->sinks().front());
        app.setStyle(QStyleFactory::create("Fusion"));
        QPalette p;
        p.setColor(QPalette::Window, QColor("#292c34"));
        p.setColor(QPalette::WindowText, QColor("#d7dae3"));
        p.setColor(QPalette::Base, QColor("#20232a"));
        p.setColor(QPalette::AlternateBase, QColor("#30343d"));
        p.setColor(QPalette::Text, QColor("#d7dae3"));
        p.setColor(QPalette::Button, QColor("#363a45"));
        p.setColor(QPalette::ButtonText, QColor("#d7dae3"));
        p.setColor(QPalette::Highlight, QColor("#7959ab"));
        p.setColor(QPalette::HighlightedText, Qt::white);
        app.setPalette(p);
        if (arguments.contains("--check-runtime")) {
            QWidget probe;
            probe.setWindowTitle("RobBench runtime check");
            probe.resize(160, 90);
            probe.createWinId();
            spdlog::info("Runtime check passed: Qt {} and native platform loaded. Vulkan device rendering "
                         "was not tested.",
                         qVersion());
            return 0;
        }
        std::atomic_bool validationFailed{false};
        QVulkanInstance instance;
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        instance.installDebugOutputFilter(
            [&validationFailed](QVulkanInstance::DebugMessageSeverityFlags severity,
                                QVulkanInstance::DebugMessageTypeFlags, const void *) {
                if (severity.testFlag(QVulkanInstance::ErrorSeverity))
                    validationFailed.store(true);
                return false;
            });
#endif
        instance.setApiVersion(QVersionNumber(1, 3, 0));
#ifndef NDEBUG
        if (instance.supportedLayers().contains("VK_LAYER_KHRONOS_validation"))
            instance.setLayers({"VK_LAYER_KHRONOS_validation"});
        else
            spdlog::warn("Vulkan validation layer unavailable");
#endif
        if (!instance.create())
            throw std::runtime_error("Vulkan 1.3 initialization failed (error " +
                                     std::to_string(instance.errorCode()) +
                                     "). Install a current Vulkan-capable graphics driver.");
        int result = 0;
        {
            rb::MainWindow window(&instance);
            window.show();
            auto args = app.arguments();
            if (args.contains("--smoke-test")) {
                int i = args.indexOf("--smoke-test");
                window.startSmokeTest(args.value(i + 1, "smoke-output"),
                                      args.contains("--smoke-rig")
                                          ? args.value(args.indexOf("--smoke-rig") + 1, "R15")
                                          : "R15");
            }
            result = app.exec();
        }
        return smoke && validationFailed.load() ? 6 : result;
    } catch (const std::exception &e) {
        spdlog::critical("{}", e.what());
        if (!smoke)
            QMessageBox::critical(nullptr, "RobBench startup failed", QString::fromUtf8(e.what()));
        return 1;
    }
}
