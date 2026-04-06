#
# Copyright (c) 2023-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_subdirectory("CodeWidget")
add_subdirectory("DockWidget")
add_subdirectory("FilterSettingsWidget")
add_subdirectory("HistoryWidget")
add_subdirectory("LineEditWidget")
add_subdirectory("PopupWidget")
if(MX_ENABLE_PYTHON_BINDINGS)
  add_subdirectory("PythonConsoleWidget")
endif()
add_subdirectory("SearchWidget")
add_subdirectory("SimpleTextInputDialog")
add_subdirectory("SpreadsheetWidget")
add_subdirectory("TabWidget")
add_subdirectory("TreeWidget")
add_subdirectory("GeneratorWidget")  # Relies on TreeWidget, SearchWidget, FilterSettingsWidget
