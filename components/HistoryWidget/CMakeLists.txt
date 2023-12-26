#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_historywidget"
  include/multiplier/ui/HistoryWidget.h
  src/HistoryWidget.cpp

  src/HistoryLabelBuilder.h
  src/HistoryLabelBuilder.cpp
)

target_sources("mx_historywidget" INTERFACE
  Resources/resources.qrc
)

target_link_libraries("mx_historywidget"
  PRIVATE
    "mx_cxxflags"

  PUBLIC
    "thirdparty_qt"
    "thirdparty_multiplier"
    "mx_util"
)

target_include_directories("mx_historywidget" PRIVATE
  "include"
)

target_include_directories("mx_historywidget" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_historywidget")
