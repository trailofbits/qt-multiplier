#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_treeexplorer"
  include/multiplier/ui/ITreeExplorer.h
  src/ITreeExplorer.cpp
  
  src/TreeExplorerItemDelegate.h
  src/TreeExplorerItemDelegate.cpp

  src/TreeExplorer.h
  src/TreeExplorer.cpp
)

target_sources("mx_treeexplorer" INTERFACE
  Resources/Resources.qrc
)

target_link_libraries("mx_treeexplorer"
  PRIVATE
    "qtmx_cxx_flags"

  PUBLIC
    "thirdparty_qt"
    "multiplier::mx-api"
    "gap::gap"
    "mx_searchwidget"
    "mx_util"
    "mx_codeview"
    "mx_globalhighlighter"
    "mx_thememanager"
    "mx_generators"
)

target_include_directories("mx_treeexplorer" PRIVATE
  "include"
)

target_include_directories("mx_treeexplorer" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_treeexplorer")
