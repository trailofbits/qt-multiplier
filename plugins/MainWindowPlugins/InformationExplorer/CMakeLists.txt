#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_information_explorer_plugin"
  include/multiplier/GUI/InformationExplorer.h

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

target_link_libraries("mx_information_explorer_plugin"
  PRIVATE
    "mx_cxx_flags"

    "mx_action_manager"
    "mx_context"

  PUBLIC
    "mx_qt_library"
    "mx_multiplier_library"

    "mx_mainwindowplugin"

    "mx_database"
    "mx_searchwidget"
    "mx_codeviewtheme"
    "mx_util_component"
    "mx_historywidget"
    "mx_globalhighlighter"
    "mx_theme_manager"
)

target_include_directories("mx_information_explorer_plugin" PRIVATE
  "include"
)

target_include_directories("mx_information_explorer_plugin" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_information_explorer_plugin")
