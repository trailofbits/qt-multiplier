#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_pythonconsole"
  
  src/PythonCodeRunner.cpp
  src/PythonCodeRunner.h

  include/multiplier/ui/PythonConsoleWidget.h
  src/PythonConsoleWidget.cpp

  src/PythonCompletionModel.cpp
  src/PythonCompletionModel.h

  src/PythonOutputAdapter.cpp
  src/PythonOutputAdapter.h
)

target_link_libraries("mx_pythonconsole"
  PRIVATE
    "mx_cxxflags"

  PUBLIC
    "thirdparty_qt"
    "thirdparty_multiplier"
    "mx_thememanager"
)

target_include_directories("mx_pythonconsole" PRIVATE
  "include"
)

target_include_directories("mx_pythonconsole" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_pythonconsole")
