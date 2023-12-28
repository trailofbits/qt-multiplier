#
# Copyright (c) 2023-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_tree_widget"
  include/multiplier/GUI/Widgets/TreeWidget.h
  src/TreeWidget.cpp
)

target_link_libraries("mx_tree_widget"
  PRIVATE
    "mx_qt_library"
)

target_include_directories("mx_tree_widget" PRIVATE
  "include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_include_directories("mx_tree_widget" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_tree_widget")
