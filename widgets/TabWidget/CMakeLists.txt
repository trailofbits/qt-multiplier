#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_tab_widget"
  include/multiplier/ui/TabWidget.h
  src/TabBar.cpp
  src/TabBar.h
  src/TabWidget.cpp

  include/multiplier/ui/MxTreeView.h
  src/MxTreeView.cpp

  include/multiplier/ui/SimpleTextInputDialog.h
  src/SimpleTextInputDialog.cpp

  include/multiplier/ui/Assert.h
  src/Assert.cpp
)

target_link_libraries("mx_tab_widget"
  PRIVATE
    "thirdparty_qt"
)

target_include_directories("mx_tab_widget" PRIVATE
  "include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_include_directories("mx_tab_widget" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_tab_widget")
