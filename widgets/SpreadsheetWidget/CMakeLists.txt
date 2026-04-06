#
# Copyright (c) 2024-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_spreadsheet_widget"
  include/multiplier/GUI/Widgets/SpreadsheetModel.h
  include/multiplier/GUI/Widgets/SpreadsheetView.h
  include/multiplier/GUI/Widgets/SpreadsheetDelegate.h
  src/SpreadsheetCommands.h
  src/SpreadsheetCommands.cpp
  src/SpreadsheetModel.cpp
  src/SpreadsheetView.cpp
  src/SpreadsheetDelegate.cpp
)

target_link_libraries("mx_spreadsheet_widget"
  PRIVATE
    "mx_cxx_flags"
    "mx_config_manager"
    "mx_theme_manager"

  PUBLIC
    "mx_qt_library"
    "mx_multiplier_library"
)

target_include_directories("mx_spreadsheet_widget" PRIVATE
  "include"
)

target_include_directories("mx_spreadsheet_widget" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_spreadsheet_widget")
