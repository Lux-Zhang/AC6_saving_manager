cmake_minimum_required(VERSION 3.21)

project(ac6dm_product_gui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Gui Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Gui Core)

add_executable(ac6dm_product_gui WIN32
    main.cpp
    product_gui_application.cpp
    main_window.cpp
    home_library_view.cpp
    import_wizard_dialog.cpp
    save_result_dialog.cpp
)

target_include_directories(ac6dm_product_gui
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(ac6dm_product_gui
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Core
)
