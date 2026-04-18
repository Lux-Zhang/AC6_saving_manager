#include "product_gui_application.hpp"

#include "main_window.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QFont>
#include <QFontDatabase>

#include <filesystem>

namespace ac6dm::app {
namespace {

void loadApplicationFont(const QString& path) {
    QFontDatabase::addApplicationFont(path);
}

}  // namespace

ProductGuiApplication::ProductGuiApplication(int& argc, char** argv)
    : app_(argc, argv), services_(AppServices::createDefault()) {
    app_.setApplicationDisplayName(QStringLiteral("AC6 Saving Manager"));
    app_.setApplicationName(QStringLiteral("AC6 Saving Manager"));
    app_.setOrganizationName(QStringLiteral("ac6dm"));
    const auto appDir = std::filesystem::path(QCoreApplication::applicationDirPath().toStdWString());
    if (std::filesystem::exists(appDir / "platforms")) {
        QCoreApplication::setLibraryPaths({QCoreApplication::applicationDirPath()});
    }

    loadApplicationFont(QStringLiteral(":/advanced_garage/fonts/aldrich-custom.ttf"));
    loadApplicationFont(QStringLiteral(":/advanced_garage/fonts/agencyfb-bold.ttf"));
    loadApplicationFont(QStringLiteral(":/advanced_garage/fonts/nhl.ttf"));

    QFont applicationFont(QStringLiteral("Aldrich-Custom"));
    applicationFont.setPointSize(12);
    applicationFont.setStyleStrategy(QFont::PreferAntialias);
    app_.setFont(applicationFont);

    QFile stylesheet(QStringLiteral(":/ui/ac6dm_dark.qss"));
    if (stylesheet.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app_.setStyleSheet(QString::fromUtf8(stylesheet.readAll()));
    }
}

ProductGuiApplication::~ProductGuiApplication() = default;

int ProductGuiApplication::run() {
    mainWindow_ = std::make_unique<MainWindow>(services_, nullptr);
    mainWindow_->show();
    return QApplication::exec();
}

}  // namespace ac6dm::app
