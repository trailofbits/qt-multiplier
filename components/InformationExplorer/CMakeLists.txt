#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_informationexplorer"
  include/multiplier/ui/InformationExplorer.h

  src/InformationExplorerModel.h
  src/InformationExplorerModel.cpp

  src/InformationExplorer.h
  src/InformationExplorer.cpp

  src/InformationExplorerTreeView.h
  src/InformationExplorerTreeView.cpp

  src/InformationExplorerItemDelegate.h
  src/InformationExplorerItemDelegate.cpp

  src/Utils.h
  src/Utils.cpp

  src/SortFilterProxyModel.h
  src/SortFilterProxyModel.cpp

  src/InformationExplorerWidget.h
  src/InformationExplorerWidget.cpp

  src/InformationExplorerPlugin.h
  src/InformationExplorerPlugin.cpp
)

target_link_libraries("mx_informationexplorer"
  PRIVATE
    "mx_cxxflags"

    "mx_actionregistry"
    "mx_context"

  PUBLIC
    "thirdparty_qt"
    "thirdparty_multiplier"

    "mx_mainwindowplugin"

    "mx_database"
    "mx_searchwidget"
    "mx_codeviewtheme"
    "mx_util"
    "mx_historywidget"
    "mx_globalhighlighter"
    "mx_thememanager"
)

target_include_directories("mx_informationexplorer" PRIVATE
  "include"
)

target_include_directories("mx_informationexplorer" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_informationexplorer")
