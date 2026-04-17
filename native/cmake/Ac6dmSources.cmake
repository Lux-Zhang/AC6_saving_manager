file(GLOB_RECURSE ac6dm_app_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/app/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/app/*.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/app/*.qrc"
)

file(GLOB_RECURSE ac6dm_core_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/config/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/config/*.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/core/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/core/*.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/platform/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/platform/*.hpp"
)
