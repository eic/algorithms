# SPDX-License-Identifier: LGPL-3.0-or-later
# Copyright (C) 2022 Whitney Armstrong, Wouter Deconinck

################################################################################
# Package: JugTrack
################################################################################

file(GLOB JugTrackPlugins_sources CONFIGURE_DEPENDS src/components/*.cpp)
add_library(JugTrackPlugins ${JugTrackPlugins_sources})
target_link_libraries(JugTrackPlugins
  JugBase
  ROOT::Core ROOT::RIO ROOT::Tree
  EDM4HEP::edm4hep
  EICD::eicd
  DD4hep::DDRec
  ActsCore
  ${genfit2}
)

target_include_directories(JugTrackPlugins PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/JugBase>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  ${genfit2_INCLUDE_DIR}
)

target_compile_options(JugTrackPlugins PRIVATE -Wno-suggest-override)
