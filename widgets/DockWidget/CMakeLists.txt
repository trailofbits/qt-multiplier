#
# Copyright (c) 2023-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_dock_widget"
  include/multiplier/GUI/Widgets/DockWidget.h
  src/DockWidget.cpp
)

target_link_libraries("mx_dock_widget"
  PRIVATE
    "mx_cxx_flags"

  PUBLIC
    "mx_qt_library"
)

target_include_directories("mx_dock_widget" PRIVATE
  "include"
)

target_include_directories("mx_dock_widget" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_dock_widget")
