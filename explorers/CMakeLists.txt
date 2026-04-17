#
# Copyright (c) 2024-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_explorers"
  include/multiplier/GUI/Explorers/CodeExplorer.h
  include/multiplier/GUI/Explorers/CodeSearchExplorer.h
  include/multiplier/GUI/Explorers/EntityExplorer.h
  include/multiplier/GUI/Explorers/HighlightExplorer.h
  include/multiplier/GUI/Explorers/NameExplorer.h
  include/multiplier/GUI/Explorers/InformationExplorer.h
  include/multiplier/GUI/Explorers/ProjectExplorer.h
  include/multiplier/GUI/Explorers/ReferenceExplorer.h
  include/multiplier/GUI/Explorers/SpreadsheetExplorer.h
  include/multiplier/GUI/Explorers/DocumentExplorer.h
  include/multiplier/GUI/Explorers/AgentExplorer.h

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

  src/Explorers/CodeSearchExplorer/CodeSearchExplorer.cpp
  src/Explorers/CodeSearchExplorer/CodeSearchResultsModel.cpp
  src/Explorers/CodeSearchExplorer/CodeSearchResultsModel.h
  src/Explorers/CodeSearchExplorer/CodeSearchRunnable.cpp
  src/Explorers/CodeSearchExplorer/CodeSearchRunnable.h

  src/Explorers/EntityExplorer/CategoryComboBox.cpp
  src/Explorers/EntityExplorer/CategoryComboBox.h
  src/Explorers/EntityExplorer/EntityExplorer.cpp

  src/Explorers/NameExplorer/NameExplorer.cpp
  src/Explorers/NameExplorer/NameRenamesModel.cpp
  src/Explorers/NameExplorer/NameRenamesModel.h
  src/Explorers/NameExplorer/RenameCommands.cpp
  src/Explorers/NameExplorer/RenameCommands.h

  src/Explorers/HighlightExplorer/HighlightedItemsModel.cpp
  src/Explorers/HighlightExplorer/HighlightedItemsModel.h
  src/Explorers/HighlightExplorer/HighlightExplorer.cpp
  src/Explorers/HighlightExplorer/HighlightThemeProxy.cpp
  src/Explorers/HighlightExplorer/HighlightThemeProxy.h
  src/Explorers/HighlightExplorer/ColorGenerator.h
  src/Explorers/HighlightExplorer/ColorGenerator.cpp

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

  src/Explorers/SpreadsheetExplorer/SpreadsheetExplorer.cpp

  src/Explorers/DocumentExplorer/DocumentExplorer.cpp

  src/Explorers/AgentExplorer/AgentExplorer.cpp
  src/Explorers/AgentExplorer/AgentConversationWidget.h
  src/Explorers/AgentExplorer/AgentConversationWidget.cpp
  src/Explorers/AgentExplorer/AgentConfigPanel.h
  src/Explorers/AgentExplorer/AgentConfigPanel.cpp
  src/Explorers/AgentExplorer/AgentDashboardWidget.h
  src/Explorers/AgentExplorer/AgentDashboardWidget.cpp
  src/Explorers/AgentExplorer/AgentToolLogWidget.h
  src/Explorers/AgentExplorer/AgentToolLogWidget.cpp
  src/Explorers/AgentExplorer/AgentSessionListWidget.h
  src/Explorers/AgentExplorer/AgentSessionListWidget.cpp
)

if(MX_ENABLE_PYTHON_BINDINGS)
  target_sources("mx_explorers" PRIVATE
    include/multiplier/GUI/Explorers/PythonConsoleExplorer.h
    src/Explorers/PythonConsoleExplorer/PythonConsoleExplorer.cpp
  )
  target_compile_definitions("mx_explorers" PUBLIC MX_ENABLE_PYTHON=1)
endif()

target_link_libraries("mx_explorers"
  PRIVATE
    "mx_action_manager"
    "mx_agent_manager"
    "mx_code_widget"
    "mx_config_manager"
    "mx_cxx_flags"
    "mx_generator_widget"
    "mx_history_widget"
    "mx_line_edit_widget"
    "mx_spreadsheet_widget"
    "mx_media_manager"
    "mx_llm_manager"
    "mx_search_widget"
    "mx_simple_text_input_dialog"
    "mx_theme_manager"
    "mx_tab_widget"
    "mx_tree_widget"
    "mx_util_component"
    $<$<BOOL:${MX_ENABLE_PYTHON_BINDINGS}>:mx_python_console_widget>

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
