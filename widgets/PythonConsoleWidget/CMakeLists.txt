#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_python_console_widget"
  
  src/PythonCodeRunner.cpp
  src/PythonCodeRunner.h

  include/multiplier/GUI/Widgets/PythonConsoleWidget.h
  src/PythonConsoleWidget.cpp

  src/PythonCompletionModel.cpp
  src/PythonCompletionModel.h

  src/PythonOutputAdapter.cpp
  src/PythonOutputAdapter.h
)

target_link_libraries("mx_python_console_widget"
  PRIVATE
    "mx_cxx_flags"

  PUBLIC
    "thirdparty_qt"
    "thirdparty_multiplier"
    "mx_thememanager"
)

target_include_directories("mx_python_console_widget" PRIVATE
  "include"
)

target_include_directories("mx_python_console_widget" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_python_console_widget")
