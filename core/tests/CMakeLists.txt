# SPDX-License-Identifier: LGPL-3.0-or-later
# Copyright (C) 2026 Wouter Deconinck

################################################################################
# Package: algorithms core utilities - tests
################################################################################

find_package(Catch2 3)
if(NOT Catch2_FOUND)
  message(WARNING "Catch2 v3 not found, skipping ${LIBRARY} tests")
  return()
endif()
include(Catch)
find_package(Threads REQUIRED)

# One executable per test source, so tests of process-wide singleton services
# (ServiceSvc, LogSvc, RandomSvc, GeoSvc) do not affect each other
file(GLOB TEST_SRC CONFIGURE_DEPENDS *.cpp)

foreach(SRC ${TEST_SRC})
  get_filename_component(NAME ${SRC} NAME_WE)
  set(TEST_NAME "${LIBRARY}_${NAME}_test")

  add_executable(${TEST_NAME} ${SRC})
  target_link_libraries(${TEST_NAME}
    PRIVATE
      algorithms::${LIBRARY}
      Catch2::Catch2WithMain
      Threads::Threads)
  target_compile_definitions(${TEST_NAME}
    PRIVATE
      TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

  # Prepend the build directory to LD_LIBRARY_PATH, so tests use the library that
  # was just built rather than an installed version found through LD_LIBRARY_PATH
  if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.22)
    set(DL_PATHS DL_PATHS $<TARGET_FILE_DIR:${LIBRARY}>)
  endif()
  catch_discover_tests(${TEST_NAME} TEST_PREFIX "${LIBRARY}/${NAME}/" ${DL_PATHS})
endforeach()
