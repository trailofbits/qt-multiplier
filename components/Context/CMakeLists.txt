#
# Copyright (c) 2023-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_context"
  include/multiplier/ui/Context.h
  src/Context.cpp
)

target_link_libraries("mx_context"
  PRIVATE
    "mx_cxxflags"
    "thirdparty_qt"

  PUBLIC
    "thirdparty_multiplier"
    
    "mx_actionregistry"
    "mx_thememanager"
)

target_include_directories("mx_context" PRIVATE
  "include"
)

target_include_directories("mx_context" SYSTEM INTERFACE
  "include"
)
