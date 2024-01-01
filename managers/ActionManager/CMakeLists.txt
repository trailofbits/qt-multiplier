#
# Copyright (c) 2023-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_action_manager"
  include/multiplier/GUI/Managers/ActionManager.h

  src/ActionManager.cpp
  src/ActionManager.h
)

target_link_libraries("mx_action_manager"
  PRIVATE
    "mx_cxx_flags"

  PUBLIC
    "mx_qt_library"
    "mx_interfaces"
)

target_include_directories("mx_action_manager" PRIVATE
  "include"
)

target_include_directories("mx_action_manager" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_action_manager")
