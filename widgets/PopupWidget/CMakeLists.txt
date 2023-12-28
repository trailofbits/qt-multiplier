#
# Copyright (c) 2023-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_popup_widget"
  include/multiplier/GUI/Widgets/PopupWidget.h
  src/PopupWidget.cpp
)

target_link_libraries("mx_popup_widget"
  PRIVATE
    "mx_cxx_flags"
    "mx_media_manager"

  PUBLIC
    "mx_qt_library"
)

target_include_directories("mx_popup_widget" PRIVATE
  "include"
)

target_include_directories("mx_popup_widget" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_popup_widget")
