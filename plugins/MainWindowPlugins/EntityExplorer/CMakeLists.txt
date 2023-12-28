#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_entity_explorer_plugin"
  include/multiplier/GUI/IEntityExplorerModel.h
  src/IEntityExplorerModel.cpp

  include/multiplier/GUI/IEntityExplorer.h
  src/IEntityExplorer.cpp

  src/EntityExplorerModel.h
  src/EntityExplorerModel.cpp

  src/EntityExplorer.h
  src/EntityExplorer.cpp

  src/EntityExplorerItemDelegate.h
  src/EntityExplorerItemDelegate.cpp

  src/CategoryComboBox.h
  src/CategoryComboBox.cpp
)

target_link_libraries("mx_entity_explorer_plugin"
  PRIVATE
    "mx_cxx_flags"

  PUBLIC
    "thirdparty_qt"
    "thirdparty_multiplier"
    "mx_util"
    "mx_searchwidget"
    "mx_database"
    "mx_codeviewtheme"
    "mx_globalhighlighter"
)

target_include_directories("mx_entity_explorer_plugin" PRIVATE
  "include"
)

target_include_directories("mx_entity_explorer_plugin" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_entity_explorer_plugin")
