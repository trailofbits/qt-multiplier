#
# Copyright (c) 2023-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_filter_settings_widget"
  include/multiplier/GUI/Widgets/FilterSettingsWidget.h
  src/FilterSettingsWidget.cpp
)

target_link_libraries("mx_filter_settings_widget"
  PRIVATE
    "mx_cxx_flags"

  PUBLIC
    "mx_interfaces"
    "mx_qt_library"
)

target_include_directories("mx_filter_settings_widget" PRIVATE
  "include"
)

target_include_directories("mx_filter_settings_widget" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_filter_settings_widget")
