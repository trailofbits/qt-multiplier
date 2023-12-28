#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_search_widget"
  include/multiplier/GUI/SearchWidget.h
  src/SearchWidget.cpp
)

target_sources("mx_search_widget" INTERFACE
  Resources/Resources.qrc
)

target_link_libraries("mx_search_widget"
  PRIVATE
    "mx_cxxflags"

  PUBLIC
    "thirdparty_qt"
    "mx_lineedit"
    "mx_util"
    "mx_thememanager"
)

target_include_directories("mx_search_widget" PRIVATE
  "include"
)

target_include_directories("mx_search_widget" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_search_widget")
