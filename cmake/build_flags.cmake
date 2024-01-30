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
  -Wno-c99-designator
)

set_target_properties("qtmx_common_flags" PROPERTIES
  INTERFACE_POSITION_INDEPENDENT_CODE
    true
)

if(MXQT_EVAL_COPY)
  set(eval_copy_flag MXQT_EVAL_COPY=1)
endif()

target_compile_definitions("qtmx_common_flags" INTERFACE
  QTMULTIPLIER_VERSION="${QTMULTIPLIER_VERSION}"
  MULTIPLIER_VERSION="${MXQT_MULTIPLIER_VERSION}"
  ${eval_copy_flag}
)

add_library("mx_cxx_flags" INTERFACE)
target_compile_features("mx_cxx_flags" INTERFACE
  cxx_std_17
)

target_link_libraries("mx_cxx_flags" INTERFACE
  "qtmx_common_flags"
)
