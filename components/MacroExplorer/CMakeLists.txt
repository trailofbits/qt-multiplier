#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_macroexplorer"
  include/multiplier/ui/IMacroExplorer.h
  src/IMacroExplorer.cpp

  src/MacroExplorer.h
  src/MacroExplorer.cpp

  src/MacroExplorerItem.h
  src/MacroExplorerItem.cpp
)

target_sources("mx_macroexplorer" INTERFACE
  Resources/Resources.qrc
)

target_link_libraries("mx_macroexplorer"
  PRIVATE
    "qtmx_cxx_flags"

  PUBLIC
    "thirdparty_qt"
    "multiplier::mx-api"
    "mx_codeview"
)

target_include_directories("mx_macroexplorer" PRIVATE
  "include"
)

target_include_directories("mx_macroexplorer" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_macroexplorer")
