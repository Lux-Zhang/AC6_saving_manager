#include "product_gui_application.hpp"

#include "main_window.hpp"

#include <QCoreApplication>
#include <QFile>

#include <filesystem>

namespace ac6dm::app {

ProductGuiApplication::ProductGuiApplication(int& argc, char** argv)
    : app_(argc, argv), services_(AppServices::createDefault()) {
    app_.setApplicationDisplayName(QStringLiteral("AC6 Saving Manager"));
    app_.setApplicationName(QStringLiteral("AC6 Saving Manager"));
    app_.setOrganizationName(QStringLiteral("ac6dm"));
    const auto appDir = std::filesystem::path(QCoreApplication::applicationDirPath().toStdWString());
    if (std::filesystem::exists(appDir / "platforms")) {
        QCoreApplication::setLibraryPaths({QCoreApplication::applicationDirPath()});
    }

    QFile stylesheet(QStringLiteral(":/ui/ac6dm_dark.qss"));
    if (stylesheet.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app_.setStyleSheet(QString::fromUtf8(stylesheet.readAll()));
    }
}

ProductGuiApplication::~ProductGuiApplication() = default;

int ProductGuiApplication::run() {
    mainWindow_ = std::make_unique<MainWindow>(services_);
    mainWindow_->show();
    return QApplication::exec();
}

}  // namespace ac6dm::app
