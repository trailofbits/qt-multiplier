#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

cmake_minimum_required(VERSION 3.19)

include("cmake/ccache.cmake")

project(qt-multiplier C CXX)

include("cmake/version.cmake")
include("cmake/utils.cmake")
include("cmake/options.cmake")
include("cmake/build_flags.cmake")
include("cmake/qt_vendor.cmake")

if(MXQT_ENABLE_INSTALL AND CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_INSTALL_PREFIX "/usr/local" CACHE PATH "" FORCE)
  else()
    set(CMAKE_INSTALL_PREFIX "/opt/multiplier" CACHE PATH "" FORCE)
  endif()
endif()

if(MXQT_ENABLE_TESTS)
  include("CTest")
  enable_testing()
endif()

# Set up Qt (system or vendor build)
mxqt_setup_qt()

# If Qt vendor build is not yet complete, only the qt6_external target
# is available. The user must build it, re-run cmake, then build again.
if(NOT MXQT_USE_SYSTEM_QT AND NOT MXQT_QT_AVAILABLE)
  message(STATUS "")
  message(STATUS "Qt is not yet available. Only the qt6_external target is defined.")
  message(STATUS "Run: cmake --build ${CMAKE_BINARY_DIR} --target qt6_external")
  message(STATUS "Then re-run cmake to configure the full project.")
  return()
endif()

add_subdirectory("libraries")

find_package("multiplier" REQUIRED GLOBAL)

add_library("mx_multiplier_library" INTERFACE)

if(MX_ENABLE_PYTHON_BINDINGS)
  target_link_libraries("mx_multiplier_library"
    INTERFACE
      Python3::Python
      "multiplier::mx-python"
  )
else()
  target_link_libraries("mx_multiplier_library"
    INTERFACE
      "multiplier::multiplier"
  )
endif()

add_library("mx_gap_library" INTERFACE)
target_link_libraries("mx_gap_library"
  INTERFACE
    "gap::gap"
    "gap::gap-core"
    "gap::gap-settings"
    "std::coroutines"
)

add_subdirectory("components")
add_subdirectory("interfaces")
add_subdirectory("managers")
add_subdirectory("themes")
add_subdirectory("widgets")
add_subdirectory("explorers")
add_subdirectory("plugins")
add_subdirectory("application")

if(MXQT_ENABLE_INSTALL)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(install_destination "share/multiplier")
  else()
    set(install_destination "control")
  endif()

  install(
    FILES "LICENSE"
    DESTINATION "${install_destination}"
    RENAME "LICENSE.txt"
  )
endif()
