if(NOT AC6DM_ENABLE_PACKAGING)
  return()
endif()

install(TARGETS ac6_data_manager_native RUNTIME DESTINATION ".")

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../packaging/third_party"
  DESTINATION "."
  OPTIONAL
)
