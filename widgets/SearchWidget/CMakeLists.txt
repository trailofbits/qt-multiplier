#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_search_widget"
  include/multiplier/GUI/Widgets/SearchWidget.h
  src/SearchWidget.cpp
)

target_link_libraries("mx_search_widget"
  PRIVATE
    "mx_cxx_flags"
    "mx_line_edit_widget"
    "mx_media_manager"

  PUBLIC
    "mx_qt_library"
)

target_include_directories("mx_search_widget" PRIVATE
  "include"
)

target_include_directories("mx_search_widget" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_search_widget")
