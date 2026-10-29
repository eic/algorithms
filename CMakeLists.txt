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

if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 20)
    message(STATUS "${PROJECT_NAME}: Setting default C++ standard to ${CMAKE_CXX_STANDARD}")
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

# Sanitizers (the runtime options used for the tests in CI are set in
# .github/workflows/linux-eic-shell.yml, with suppressions in .github/*.supp)
include(CheckCXXCompilerFlag)

# Address sanitizer
option(USE_ASAN "Compile with address sanitizer" OFF)
if(USE_ASAN)
  add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g -O1)
  add_link_options(-fsanitize=address)
endif()

# Thread sanitizer
option(USE_TSAN "Compile with thread sanitizer" OFF)
if(USE_TSAN)
  if(USE_ASAN)
    message(FATAL_ERROR "USE_TSAN cannot be combined with USE_ASAN")
  endif()
  add_compile_options(-fsanitize=thread -g -O1)
  add_link_options(-fsanitize=thread)
endif()

# Undefined behavior sanitizer
option(USE_UBSAN "Compile with undefined behavior sanitizer" OFF)
if(USE_UBSAN)
  foreach(sanitizer undefined float-divide-by-zero unsigned-integer-overflow
                    implicit-conversion local-bounds nullability)
    check_cxx_compiler_flag("-fsanitize=${sanitizer}" CXX_COMPILER_HAS_sanitize_${sanitizer})
    if(CXX_COMPILER_HAS_sanitize_${sanitizer})
      add_compile_options(-fsanitize=${sanitizer})
      add_link_options(-fsanitize=${sanitizer})
    endif()
  endforeach()
  add_compile_options(-fno-omit-frame-pointer -g -O1)
endif()

# Install to the top directory by default
if( ${CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT} )
    set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR} CACHE PATH "Install in top directory by default" FORCE)
endif()

find_package(Microsoft.GSL CONFIG)
find_package(EDM4EIC REQUIRED)
find_package(EDM4HEP REQUIRED)
if(${EDM4HEP_VERSION} VERSION_LESS 0.4.1)
  message(FATAL_ERROR "EDM4HEP version ${EDM4HEP_VERSION} is too old. Please use at least version 0.4.1.")
endif()
find_package(DD4hep COMPONENTS DDRec REQUIRED)
find_package(fmt REQUIRED)

# Tests (require Catch2 v3, skipped with a warning if not found)
option(BUILD_TESTING "Build the tests" ON)
if(BUILD_TESTING)
  enable_testing()
endif()

include(GNUInstallDirs)
include(algorithmsComponentsHelpers) # handle components via add_..._if commands

add_component(core Core)

# create cmake configuration files
include(algorithmsCreatePackageConfig)
