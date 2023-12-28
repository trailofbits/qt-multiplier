#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_actionregistry"

  include/multiplier/GUI/Managers/ActionManager.h

  src/ActionManager.cpp
  src/Managers/ActionManager.h
  src/IAction.cpp
)

target_link_libraries("mx_actionregistry"
  PRIVATE
    "mx_cxx_flags"

  PUBLIC
    "thirdparty_qt"
    "thirdparty_multiplier"
)

target_include_directories("mx_actionregistry" PRIVATE
  "include"
)

target_include_directories("mx_actionregistry" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_actionregistry")
