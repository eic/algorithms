# SPDX-License-Identifier: LGPL-3.0-or-later
# Copyright (C) 2022 Wouter Deconinck, Sylvester Joosten

################################################################################
# Package: algorithms far_forward utilities
################################################################################

set(SUBDIR "far_forward")
set(LIBRARY "${SUBDIR}")
set(TARGETS ${TARGETS} ${LIBRARY} PARENT_SCOPE)

# FIXME: adding one by one
#file(GLOB SRC CONFIGURE_DEPENDS src/*.cpp)
set(SRC
)

add_library(${LIBRARY} SHARED ${SRC})
add_library(algorithms::${LIBRARY} ALIAS ${LIBRARY})
target_link_libraries(${LIBRARY}
  PUBLIC
    EDM4HEP::edm4hep
    EDM4EIC::edm4eic
    DD4hep::DDRec
    algorithms::core
    fmt::fmt)
target_include_directories(${LIBRARY}
  PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/${SUBDIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
set_target_properties(${LIBRARY} PROPERTIES
  OUTPUT_NAME algo${SUBDIR}
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})

install(TARGETS ${LIBRARY}
  EXPORT algorithmsFarForwardTargets
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT bin
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT shlib
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib
  INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

install(DIRECTORY ${PROJECT_SOURCE_DIR}/${SUBDIR}/include/algorithms
DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT dev)

# TODO: Testing
#if(BUILD_TESTING)
#  enable_testing()
#endif()

