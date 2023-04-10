#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

cmake_minimum_required(VERSION 3.19)

include("cmake/ccache.cmake")

project(qt-multiplier CXX)

include("cmake/version.cmake")
include("cmake/utils.cmake")
include("cmake/options.cmake")
include("cmake/build_flags.cmake")

if(MXQT_ENABLE_TESTS)
  include("CTest")
  enable_testing()
endif()

add_subdirectory("libraries")
add_subdirectory("components")
add_subdirectory("multiplier")

if(MXQT_ENABLE_INSTALL)
  install(
    FILES "LICENSE"
    DESTINATION "control"
    RENAME "LICENSE.txt"
  )
endif()
