
macro(aax_sources objs)
  set(SOURCES "")
  foreach(src ${objs})
    set(SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${src};${SOURCES}")
  endforeach()
endmacro()

macro(aax_subdirectory dir)
  add_subdirectory(${dir})
  get_directory_property(OBJECTS DIRECTORY ${dir} DEFINITION SOURCES)
  set(SOURCES "${OBJECTS};${SOURCES}")
endmacro()

# Generates <Package>Config.cmake and <Package>Version.cmake.
if(PACKAGING_INCLUDED)
  return()
endif()
set(PACKAGING_INCLUDED true)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

set(PACKAGING_MODULE_DIR "${PROJECT_SOURCE_DIR}/CMakeModules")
set(CMAKE_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")

# Generates <Package>Config.cmake.
configure_package_config_file(
  "${PACKAGING_MODULE_DIR}/PackageConfig.cmake.in"
  "${PROJECT_NAME}Config.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_CMAKEDIR}")

# Generates <Package>Version.cmake.
write_basic_package_version_file(
  "${PROJECT_NAME}ConfigVersion.cmake"
  VERSION ${AAX_VERSION}
  COMPATIBILITY SameMajorVersion)

install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_CMAKEDIR}" COMPONENT Headers)

