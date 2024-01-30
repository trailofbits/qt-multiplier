#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_history_widget"
  include/multiplier/GUI/Widgets/HistoryWidget.h
  src/HistoryWidget.cpp

  src/HistoryLabelBuilder.h
  src/HistoryLabelBuilder.cpp
)

target_link_libraries("mx_history_widget"
  PRIVATE
    "mx_code_widget"
    "mx_config_manager"
    "mx_cxx_flags"
    "mx_media_manager"
    "mx_util_component"

  PUBLIC
    "mx_qt_library"
    "mx_multiplier_library"
)

target_include_directories("mx_history_widget" PRIVATE
  "include"
)

target_include_directories("mx_history_widget" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_history_widget")
