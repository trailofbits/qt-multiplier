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

include("cmake/ccache.cmake")
include("cmake/utils.cmake")
include("cmake/options.cmake")
include("cmake/build_flags.cmake")
include("cmake/install.cmake")

add_subdirectory("libraries")
add_subdirectory("components")
add_subdirectory("multiplier")
