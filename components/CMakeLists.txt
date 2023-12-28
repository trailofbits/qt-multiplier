#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_subdirectory("Util")
add_subdirectory("ActionManager")
add_subdirectory("ThemeManager")
add_subdirectory("Context")
add_subdirectory("MainWindowPlugin")

add_subdirectory("CodeView")
add_subdirectory("CodeViewTheme")
add_subdirectory("Database")
add_subdirectory("FontDatabase")
add_subdirectory("SearchWidget")
add_subdirectory("HistoryWidget")
add_subdirectory("LineEdit")
add_subdirectory("TokenLabel")
add_subdirectory("Generators")

if(MX_ENABLE_PYTHON_BINDINGS)
  add_subdirectory("PythonConsoleWidget")
endif()
