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

add_subdirectory("libraries")

find_package("multiplier" REQUIRED GLOBAL)

add_library("thirdparty_multiplier" INTERFACE)

if(MX_ENABLE_PYTHON_BINDINGS)
  find_dependency(Python3 REQUIRED COMPONENTS Interpreter Development.Embed)
  target_link_libraries("thirdparty_multiplier"
    INTERFACE
      Python3::Python
      "multiplier::mx-python"
  )
else()
  target_link_libraries("thirdparty_multiplier"
    INTERFACE
      "multiplier::multiplier"
  )
endif(MX_ENABLE_PYTHON_BINDINGS)

add_subdirectory("components")
add_subdirectory("widgets")
add_subdirectory("plugins")
add_subdirectory("multiplier")

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
