#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_searchwidget"
  include/multiplier/ui/ISearchWidget.h
  src/ISearchWidget.cpp

  src/SearchWidget.h
  src/SearchWidget.cpp
)

target_sources("mx_searchwidget" INTERFACE
  Resources/Resources.qrc
)

target_link_libraries("mx_searchwidget"
  PRIVATE
    "qtmx_cxx_flags"

  PUBLIC
    "thirdparty_qt"
    "mx_assert"
    "mx_lineedit"
)

target_include_directories("mx_searchwidget" PRIVATE
  "include"
)

target_include_directories("mx_searchwidget" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_searchwidget")
