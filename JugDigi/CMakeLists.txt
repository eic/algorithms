# SPDX-License-Identifier: LGPL-3.0-or-later
# Copyright (C) 2022 Wouter Deconinck, Whitney Armstrong, Chao Peng

################################################################################
# Package: JugDigi
################################################################################

file(GLOB JugDigiPlugins_sources CONFIGURE_DEPENDS src/components/*.cpp)
add_library(JugDigiPlugins ${JugDigiPlugins_sources})
target_link_libraries(JugDigiPlugins
  ROOT::Core ROOT::RIO ROOT::Tree
  EDM4HEP::edm4hep
  EICD::eicd
)

target_include_directories(JugDigiPlugins PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/JugBase>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_compile_options(JugDigiPlugins PRIVATE -Wno-suggest-override)

install(TARGETS JugDigiPlugins
  EXPORT ${PROJECT_NAME}Targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/JugDigi
)
