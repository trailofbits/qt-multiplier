#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_entityexplorer"
  include/multiplier/ui/IEntityExplorerModel.h
  src/IEntityExplorerModel.cpp

  include/multiplier/ui/IEntityExplorer.h
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

target_link_libraries("mx_entityexplorer"
  PRIVATE
    "mx_cxxflags"

  PUBLIC
    "thirdparty_qt"
    "thirdparty_multiplier"
    "mx_util"
    "mx_searchwidget"
    "mx_database"
    "mx_codeviewtheme"
    "mx_globalhighlighter"
)

target_include_directories("mx_entityexplorer" PRIVATE
  "include"
)

target_include_directories("mx_entityexplorer" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_entityexplorer")
