#
# Copyright (c) 2024-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_config_editor"
  include/multiplier/GUI/Widgets/ConfigEditor.h

  src/ConfigEditor.h
  src/ConfigEditor.cpp

  src/ConfigModel.h
  src/ConfigModel.cpp

  src/ConfigEditorDelegate.h
  src/ConfigEditorDelegate.cpp
)

target_link_libraries("mx_config_editor"
  PUBLIC
    "mx_tree_widget"

  PRIVATE
    "mx_config_manager"
    "mx_cxx_flags"
)

target_include_directories("mx_config_editor" PRIVATE
  "include"
)

target_include_directories("mx_config_editor" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_config_editor")
