#
# Copyright (c) 2024-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_code_widget"
  include/multiplier/GUI/Widgets/CodeWidget.h

  src/CodeModel.cpp
  src/CodeModel.h
  src/CodeWidget.cpp
)

target_link_libraries("mx_code_widget"
  PRIVATE
    "mx_config_manager"
    "mx_cxx_flags"
    "mx_interfaces"
    "mx_media_manager"
    "mx_theme_manager"

  PUBLIC
    "mx_multiplier_library"
    "mx_qt_library"
)

target_include_directories("mx_code_widget" PRIVATE
  "include"
)

target_include_directories("mx_code_widget" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_code_widget")
