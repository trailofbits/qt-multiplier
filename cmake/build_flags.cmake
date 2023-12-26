#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("qtmx_common_flags" INTERFACE)
target_compile_options("qtmx_common_flags" INTERFACE
  -Wall
  -pedantic
  -Wconversion
  -Wunused
  -Wshadow
  -fvisibility=hidden
  -Werror
)

set_target_properties("qtmx_common_flags" PROPERTIES
  INTERFACE_POSITION_INDEPENDENT_CODE
    true
)

target_compile_definitions("qtmx_common_flags" INTERFACE
  QTMULTIPLIER_VERSION="${QTMULTIPLIER_VERSION}"
  MULTIPLIER_VERSION="${MXQT_MULTIPLIER_VERSION}"
)

add_library("mx_cxxflags" INTERFACE)
target_compile_features("mx_cxxflags" INTERFACE
  cxx_std_17
)

target_link_libraries("mx_cxxflags" INTERFACE
  "qtmx_common_flags"
)
