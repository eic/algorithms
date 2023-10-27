# Create relocatable CMake package config files

include(CMakePackageConfigHelpers)

# use path suggested by
# https://cmake.org/cmake/help/v3.18/manual/cmake-packages.7.html
set(install_package_config_dir "${CMAKE_INSTALL_LIBDIR}/cmake/algorithms")

# version is taken automatically from PROJECT_VERSION; no need to specify
write_basic_package_version_file(
  ${PROJECT_BINARY_DIR}/algorithmsConfigVersion.cmake
  COMPATIBILITY SameMajorVersion)
configure_package_config_file(
  ${CMAKE_CURRENT_LIST_DIR}/algorithmsConfig.cmake.in
  ${PROJECT_BINARY_DIR}/algorithmsConfig.cmake
  INSTALL_DESTINATION ${install_package_config_dir}
  PATH_VARS CMAKE_INSTALL_BINDIR CMAKE_INSTALL_INCLUDEDIR CMAKE_INSTALL_LIBDIR)

# install cmake package configs
install(
  FILES
    ${PROJECT_BINARY_DIR}/algorithmsConfigVersion.cmake
    ${PROJECT_BINARY_DIR}/algorithmsConfig.cmake
  DESTINATION ${install_package_config_dir})

# install target configs for all available components
foreach(_component ${_components})
  install(
    EXPORT algorithms${_component}Targets
    NAMESPACE algorithms::
    DESTINATION ${install_package_config_dir})
endforeach()
