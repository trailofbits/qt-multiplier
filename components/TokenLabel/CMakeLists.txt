#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_tokenlabel"
  include/multiplier/GUI/ITokenLabel.h
  src/ITokenLabel.cpp

  src/TokenLabel.h
  src/TokenLabel.cpp
)

target_link_libraries("mx_tokenlabel"
  PRIVATE
    "mx_cxx_flags"
    "mx_qt_library"

  PUBLIC
    "mx_multiplier_library"
    "mx_util_component"
    "mx_theme_manager"
)

target_include_directories("mx_tokenlabel" PRIVATE
  "include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_include_directories("mx_tokenlabel" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_tokenlabel")
