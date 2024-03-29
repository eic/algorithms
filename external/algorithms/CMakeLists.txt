# SPDX-License-Identifier: LGPL-3.0-or-later
# Copyright (C) 2022 Whitney Armstrong, Wouter Deconinck, Sylvester Joosten

cmake_minimum_required(VERSION 3.19)

# CMP0074: find_package() uses <PackageName>_ROOT variables
cmake_policy(SET CMP0074 NEW)

project(algorithms VERSION 1.1.0)

set(CMAKE_CXX_STANDARD 17 CACHE STRING "")
if(NOT CMAKE_CXX_STANDARD MATCHES "17|20")
  message(FATAL_ERROR "Unsupported C++ standard: ${CMAKE_CXX_STANDARD}")
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands as json for run-clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Also use clang-tidy integration in CMake
option(ENABLE_CLANG_TIDY "Enable clang-tidy integration in cmake" OFF)
if(ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
  if (CLANG_TIDY_EXE)
    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}" CACHE STRING "" FORCE)
  else()
    set(CMAKE_CXX_CLANG_TIDY "" CACHE STRING "" FORCE)
  endif()
endif()

# Set default build type
set(default_build_type "Release")
if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
  set(default_build_type "RelWithDebInfo")
endif()
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Set all warnings
if(NOT CMAKE_BUILD_TYPE MATCHES Release)
  add_compile_options(-Wall -Wextra -Werror)
endif()

find_package(Microsoft.GSL CONFIG)

find_package(EDM4EIC REQUIRED)
find_package(EDM4HEP 0.4.1 REQUIRED)
#find_package(DD4hep COMPONENTS DDG4 DDG4IO DDRec REQUIRED)
find_package(DD4hep COMPONENTS DDRec REQUIRED)
find_package(fmt REQUIRED)

include(GNUInstallDirs)

add_subdirectory(core)

# FIXME: add directory as soon as one algorithm converted
#add_subdirectory(acts)
add_subdirectory(calorimetry)
#add_subdirectory(dis)
#add_subdirectory(far_forward)
#add_subdirectory(pid)
#add_subdirectory(tracking)
add_subdirectory(truth)
#add_subdirectory(utility)

# -------------------------
# install library config
include(CMakePackageConfigHelpers)

# Install and export targets
install(EXPORT ${PROJECT_NAME}Targets
  FILE ${PROJECT_NAME}Targets.cmake
  NAMESPACE ${PROJECT_NAME}::
  DESTINATION lib/${PROJECT_NAME}
  )

set(TARGETS_INSTALL_PATH lib/${PROJECT_NAME}/${PROJECT_NAME}Targets.cmake)

configure_package_config_file(
  cmake/${PROJECT_NAME}Config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
  INSTALL_DESTINATION lib/${PROJECT_NAME}
  PATH_VARS TARGETS_INSTALL_PATH
  )

write_basic_package_version_file(
  ${PROJECT_NAME}ConfigVersion.cmake
  VERSION ${VERSION}
  COMPATIBILITY SameMajorVersion
  )

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
  DESTINATION lib/${PROJECT_NAME}
  )
