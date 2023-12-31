#
# Copyright (c) 2023-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_config_manager"
  include/multiplier/GUI/Managers/ConfigManager.h
  src/ConfigManager.cpp

  src/ThemedItemDelegate.cpp
  src/ThemedItemDelegate.h
)

target_link_libraries("mx_config_manager"
  PRIVATE
    "mx_cxx_flags"
    "mx_multiplier_library"
    "mx_action_manager"
    "mx_theme_manager"
    "mx_media_manager"

  PUBLIC
    "mx_qt_library"
)

target_include_directories("mx_config_manager" PRIVATE
  "include"
)

target_include_directories("mx_config_manager" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_config_manager")

