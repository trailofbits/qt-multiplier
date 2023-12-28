#
# Copyright (c) 2023-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_context"
  include/multiplier/GUI/Context.h
  src/Context.cpp
)

target_link_libraries("mx_context"
  PRIVATE
    "mx_cxx_flags"
    "mx_qt_library"

  PUBLIC
    "mx_multiplier_library"
    
    "mx_action_manager"
    "mx_theme_manager"
)

target_include_directories("mx_context" PRIVATE
  "include"
)

target_include_directories("mx_context" SYSTEM INTERFACE
  "include"
)
