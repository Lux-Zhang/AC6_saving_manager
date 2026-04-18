#pragma once

#include "app_services.hpp"

#include <QApplication>
#include <memory>
#include <optional>
#include <filesystem>

namespace ac6dm::app {

class MainWindow;

class ProductGuiApplication final {
public:
    ProductGuiApplication(int& argc, char** argv);
    ~ProductGuiApplication();
    int run();

private:
    QApplication app_;
    AppServices services_;
    std::unique_ptr<MainWindow> mainWindow_;
    std::optional<std::filesystem::path> startupSavePath_{};
    std::optional<int> startupAcRow_{};
};

}  // namespace ac6dm::app
