# SPDX-License-Identifier: LGPL-3.0-or-later
# Copyright (C) 2022 Wouter Deconinck, Whitney Armstrong

################################################################################
# Package: JugBase
################################################################################

file(GLOB JugBasePlugins_sources CONFIGURE_DEPENDS src/components/*.cpp)
set(JugBasePlugins_sources
  src/components/Algorithm.cpp
)
file(GLOB JugBasePlugins_headers CONFIGURE_DEPENDS JugBase/*.h)
add_library(JugBasePlugins SHARED "${JugBasePlugins_sources}")
set_target_properties(JugBasePlugins
  PROPERTIES PUBLIC_HEADER "${JugBasePlugins_headers}")
target_link_libraries(JugBasePlugins
  EDM4HEP::edm4hep
  EICD::eicd
)

target_include_directories(JugBasePlugins PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/JugBase>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_compile_options(JugBasePlugins PRIVATE -Wno-suggest-override)

install(TARGETS JugBasePlugins
  EXPORT ${PROJECT_NAME}Targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/JugBase
)
