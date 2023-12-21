#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_tokenlabel"
  include/multiplier/ui/ITokenLabel.h
  src/ITokenLabel.cpp

  src/TokenLabel.h
  src/TokenLabel.cpp
)

target_link_libraries("mx_tokenlabel"
  PRIVATE
    "qtmx_cxx_flags"
    "thirdparty_qt"

  PUBLIC
    "thirdparty_multiplier"
    "mx_util"
    "mx_thememanager"
)

target_include_directories("mx_tokenlabel" PRIVATE
  "include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_include_directories("mx_tokenlabel" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_tokenlabel")
