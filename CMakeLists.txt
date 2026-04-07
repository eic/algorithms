# SPDX-License-Identifier: LGPL-3.0-or-later
# Copyright (C) 2022 Whitney Armstrong, Wouter Deconinck, Sylvester Joosten

cmake_minimum_required(VERSION 3.19)

# CMP0074: find_package() uses <PackageName>_ROOT variables
cmake_policy(SET CMP0074 NEW)

# must be set before project(...) call; version module is needed before
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# determine project version; sets _algorithms_version
include(algorithmsRetrieveVersion)

project(algorithms VERSION ${_algorithms_version} LANGUAGES CXX)

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
  add_compile_options(-Wall -Wextra -Werror -Wno-error=deprecated-declarations)
endif()

# Install to the top directory by default
if( ${CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT} )
    set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR} CACHE PATH "Install in top directory by default" FORCE)
endif()

find_package(Microsoft.GSL CONFIG)
find_package(EDM4EIC REQUIRED)
find_package(EDM4HEP 0.4.1 REQUIRED)
find_package(DD4hep COMPONENTS DDRec REQUIRED)
find_package(fmt REQUIRED)

include(GNUInstallDirs)
include(algorithmsComponentsHelpers) # handle components via add_..._if commands

add_component(core Core)

# FIXME: add directory as soon as one algorithm converted
#add_component(acts Acts)
add_component(calorimetry Calorimetry)
#add_component(dis DIS)
#add_component(far_forward Far_Forward)
#add_component(pid PID)
#add_component(tracking Tracking)
add_component(truth Truth)
#add_component(utility Utility)

# create cmake configuration files
include(algorithmsCreatePackageConfig)
