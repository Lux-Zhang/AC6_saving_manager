cmake_minimum_required(VERSION 3.24)

project(ac6_data_manager_native VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(Ac6dmOptions)
include(Ac6dmSources)

if(MSVC)
  add_compile_options(/utf-8)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets)
find_package(OpenSSL REQUIRED COMPONENTS Crypto)
find_package(ZLIB REQUIRED)

if(AC6DM_BUILD_TESTS)
  enable_testing()
  find_package(GTest QUIET)
endif()

set(ac6dm_common_headers
  include/ac6dm/contracts/common_types.hpp
  include/ac6dm/contracts/emblem_workflow.hpp
  include/ac6dm/contracts/service_interfaces.hpp
  include/ac6dm/contracts/evidence_contracts.hpp
  include/ac6dm/contracts/error_contracts.hpp
  include/ac6dm/contracts/slot_contracts.hpp
)

add_executable(ac6_data_manager_native WIN32
  ${ac6dm_app_sources}
  ${ac6dm_core_sources}
  ${ac6dm_common_headers}
)

set_target_properties(ac6_data_manager_native PROPERTIES
  OUTPUT_NAME "AC6 saving manager"
)

target_include_directories(ac6_data_manager_native
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(ac6_data_manager_native
  PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    OpenSSL::Crypto
    ZLIB::ZLIB
)

target_compile_definitions(ac6_data_manager_native
  PRIVATE
    AC6DM_APP_NAME=\"AC6 saving manager\"
)

if(MSVC)
  target_compile_options(ac6_data_manager_native PRIVATE /W4 /permissive- /EHsc)
else()
  target_compile_options(ac6_data_manager_native PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(AC6DM_BUILD_TESTS)
  add_subdirectory(tests)
endif()

include(Ac6dmInstall)
