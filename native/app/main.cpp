#include "app_services.hpp"
#include "product_gui_application.hpp"

#include <QFile>
#include <QString>

#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

std::filesystem::path executableDirFromArgv(char** argv) {
    if (argv == nullptr || argv[0] == nullptr) {
        return std::filesystem::current_path();
    }
    const auto exePath = std::filesystem::path(QString::fromLocal8Bit(argv[0]).toStdWString());
    return exePath.has_parent_path() ? exePath.parent_path() : std::filesystem::current_path();
}

void ensureQtConf(const std::filesystem::path& appDir) {
    const auto qtConfPath = appDir / "qt.conf";
    if (std::filesystem::exists(qtConfPath)) {
        return;
    }
    QFile qtConf(QString::fromStdWString(qtConfPath.wstring()));
    if (!qtConf.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return;
    }
    const QByteArray payload =
        "[Paths]\n"
        "Prefix = .\n"
        "Plugins = .\n"
        "Translations = translations\n";
    qtConf.write(payload);
}

void isolateQtRuntime(char** argv) {
    const auto appDir = executableDirFromArgv(argv);
    const auto platformsDir = appDir / "platforms";
    const auto stylesDir = appDir / "styles";
    const auto imageFormatsDir = appDir / "imageformats";
    const bool hasBundledQtPlugins = std::filesystem::exists(platformsDir);

    if (hasBundledQtPlugins) {
        ensureQtConf(appDir);
        qputenv("QT_PLUGIN_PATH", QByteArray());
        qputenv("QT_QPA_PLATFORM_PLUGIN_PATH", QString::fromStdWString(platformsDir.wstring()).toLocal8Bit());
        qputenv("QT_QPA_GENERIC_PLUGINS", QByteArray());
        qputenv("QT_QPA_PLATFORMTHEME", QByteArray());
        qputenv("QT_STYLE_OVERRIDE", QByteArray());
        qputenv("QT_QPA_FONTDIR", QByteArray());
        qputenv("AC6DM_QT_APPDIR", QString::fromStdWString(appDir.wstring()).toLocal8Bit());
        qputenv("AC6DM_QT_STYLESDIR", QString::fromStdWString(stylesDir.wstring()).toLocal8Bit());
        qputenv("AC6DM_QT_IMAGEFORMATSDIR", QString::fromStdWString(imageFormatsDir.wstring()).toLocal8Bit());
    }

#ifdef _WIN32
    ::SetDllDirectoryW(appDir.c_str());
#endif
}

}  // namespace

int main(int argc, char** argv) {
    isolateQtRuntime(argv);
    ac6dm::app::ProductGuiApplication application(argc, argv);
    return application.run();
}
