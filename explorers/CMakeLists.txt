#
# Copyright (c) 2024-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_explorers"
  include/multiplier/GUI/Explorers/CodeExplorer.h
  include/multiplier/GUI/Explorers/EntityExplorer.h
  include/multiplier/GUI/Explorers/HighlightExplorer.h
  include/multiplier/GUI/Explorers/InformationExplorer.h
  include/multiplier/GUI/Explorers/ProjectExplorer.h
  include/multiplier/GUI/Explorers/ReferenceExplorer.h

  include/multiplier/GUI/Interfaces/IInfoGenerator.h
  include/multiplier/GUI/Interfaces/IInformationExplorerPlugin.h
  include/multiplier/GUI/Interfaces/IMainWindowPlugin.h
  include/multiplier/GUI/Interfaces/IReferenceExplorerPlugin.h

  src/Interfaces/IInfoGenerator.cpp
  src/Interfaces/IInformationExplorerPlugin.cpp
  src/Interfaces/IMainWindowPlugin.cpp
  src/Interfaces/IReferenceExplorerPlugin.cpp

  src/Explorers/CodeExplorer/CodeExplorer.cpp
  src/Explorers/CodeExplorer/CodePreviewWidget.cpp
  src/Explorers/CodeExplorer/CodePreviewWidget.h
  src/Explorers/CodeExplorer/ExpandedMacrosModel.cpp
  src/Explorers/CodeExplorer/ExpandedMacrosModel.h
  src/Explorers/CodeExplorer/MacroExplorer.cpp
  src/Explorers/CodeExplorer/MacroExplorer.h

  src/Explorers/EntityExplorer/CategoryComboBox.cpp
  src/Explorers/EntityExplorer/CategoryComboBox.h
  src/Explorers/EntityExplorer/EntityExplorer.cpp

  src/Explorers/HighlightExplorer/HighlightedItemsModel.cpp
  src/Explorers/HighlightExplorer/HighlightedItemsModel.h
  src/Explorers/HighlightExplorer/HighlightExplorer.cpp
  src/Explorers/HighlightExplorer/HighlightThemeProxy.cpp
  src/Explorers/HighlightExplorer/HighlightThemeProxy.h

  src/Explorers/InformationExplorer/EntityInformationModel.cpp
  src/Explorers/InformationExplorer/EntityInformationModel.h
  src/Explorers/InformationExplorer/EntityInformationRunnable.cpp
  src/Explorers/InformationExplorer/EntityInformationRunnable.h
  src/Explorers/InformationExplorer/EntityInformationWidget.cpp
  src/Explorers/InformationExplorer/EntityInformationWidget.h
  src/Explorers/InformationExplorer/InformationExplorer.cpp
  src/Explorers/InformationExplorer/SortFilterProxyModel.cpp
  src/Explorers/InformationExplorer/SortFilterProxyModel.h

  src/Explorers/ProjectExplorer/FileTreeModel.cpp
  src/Explorers/ProjectExplorer/FileTreeModel.h
  src/Explorers/ProjectExplorer/FileTreeView.cpp
  src/Explorers/ProjectExplorer/FileTreeView.h
  src/Explorers/ProjectExplorer/ProjectExplorer.cpp

  src/Explorers/ReferenceExplorer/ReferenceExplorer.cpp
)

target_link_libraries("mx_explorers"
  PRIVATE
    "mx_action_manager"
    "mx_code_widget"
    "mx_config_manager"
    "mx_cxx_flags"
    "mx_generator_widget"
    "mx_history_widget"
    "mx_line_edit_widget"
    "mx_media_manager"
    "mx_search_widget"
    "mx_simple_text_input_dialog"
    "mx_theme_manager"
    "mx_tab_widget"
    "mx_tree_widget"
    "mx_util_component"

  PUBLIC
    "mx_gap_library"
    "mx_multiplier_library"
    "mx_qt_library"
)

target_include_directories("mx_explorers" PRIVATE
  "include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_include_directories("mx_explorers" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_explorers")
