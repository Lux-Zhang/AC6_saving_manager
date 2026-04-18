if(NOT GTest_FOUND)
  message(STATUS "GTest not found; native tests will be skipped in this environment.")
  return()
endif()

add_executable(ac6dm_native_tests
  contracts_smoke_test.cpp
  emblem_catalog_contract_test.cpp
  ac_catalog_contract_test.cpp
  exchange_package_codec_test.cpp
  exchange_service_workflow_test.cpp
  manifest_gate_contract_test.cpp
  staging_policy_contract_test.cpp
  import_constraints_contract_test.cpp
  preview_contract_test.cpp
  emblem_preview_service_test.cpp
  ac_preview_probe_test.cpp
  ac_preview_support_test.cpp
  ac_regulation_catalog_test.cpp
  ac_runtime_regulation_test.cpp
  ui/home_library_view_test.cpp
  ui/target_slot_dialog_test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../app/home_library_view.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../app/native_exchange_workflow.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../app/target_slot_dialog.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../app/ui_resources.qrc
  ${ac6dm_core_sources}
)

target_include_directories(ac6dm_native_tests
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../include"
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_link_libraries(ac6dm_native_tests
  PRIVATE
    GTest::gtest_main
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    OpenSSL::Crypto
    ZLIB::ZLIB
)

include(GoogleTest)
gtest_discover_tests(ac6dm_native_tests)
