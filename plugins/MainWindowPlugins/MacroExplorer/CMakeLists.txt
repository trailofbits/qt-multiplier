#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_macro_explorer_plugin"
  include/multiplier/GUI/IMacroExplorer.h
  src/IMacroExplorer.cpp

  src/MacroExplorer.h
  src/MacroExplorer.cpp

  src/MacroExplorerItem.h
  src/MacroExplorerItem.cpp
)

target_sources("mx_macro_explorer_plugin" INTERFACE
  Resources/Resources.qrc
)

target_link_libraries("mx_macro_explorer_plugin"
  PRIVATE
    "mx_cxx_flags"

  PUBLIC
    "thirdparty_qt"
    "thirdparty_multiplier"
    "mx_codeview"
)

target_include_directories("mx_macro_explorer_plugin" PRIVATE
  "include"
)

target_include_directories("mx_macro_explorer_plugin" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_macro_explorer_plugin")
