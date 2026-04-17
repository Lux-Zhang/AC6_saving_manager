#pragma once

#include "app_services.hpp"

#include <QApplication>
#include <memory>

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
};

}  // namespace ac6dm::app
