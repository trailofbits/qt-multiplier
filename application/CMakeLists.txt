#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

cmake_minimum_required(VERSION 3.19)

set(MACOSX_BUNDLE_ICON_FILE
  "logo.icns"
)

set(multiplier_icon_path
  "${PROJECT_SOURCE_DIR}/managers/MediaManager/resources/Icons/${MACOSX_BUNDLE_ICON_FILE}"
)

set_source_files_properties("${multiplier_icon_path}" PROPERTIES
  MACOSX_PACKAGE_LOCATION "Resources"
)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(executable_type "MACOSX_BUNDLE")
endif()

add_executable("Multiplier" ${executable_type}
  src/Main.cpp

  include/multiplier/GUI/Interfaces/IMainWindowPlugin.h
  src/Interfaces/IMainWindowPlugin.cpp

  src/MainWindow.h
  src/MainWindow.cpp

  src/MetaTypes.h
  src/MetaTypes.cpp
  
  src/MultiplierStyle.h
  src/MultiplierStyle.cpp

  src/Explorers/EntityExplorer/CategoryComboBox.cpp
  src/Explorers/EntityExplorer/CategoryComboBox.h
  src/Explorers/EntityExplorer/EntityExplorer.cpp
  src/Explorers/EntityExplorer/EntityExplorer.h

  src/Explorers/ProjectExplorer/FileTreeModel.cpp
  src/Explorers/ProjectExplorer/FileTreeModel.h
  src/Explorers/ProjectExplorer/FileTreeView.cpp
  src/Explorers/ProjectExplorer/FileTreeView.h
  src/Explorers/ProjectExplorer/ProjectExplorer.cpp
  src/Explorers/ProjectExplorer/ProjectExplorer.h
)

target_sources("Multiplier" PRIVATE
  Info.plist
  "${multiplier_icon_path}"
)

target_link_libraries("Multiplier"
  PRIVATE
    "mx_action_manager"
    "mx_builtin_theme"
    "mx_config_manager"
    "mx_cxx_flags"
    "mx_generator_widget"
    "mx_media_manager"
    "mx_multiplier_library"
    "mx_phantomstyle_library"
    "mx_search_widget"
    "mx_theme_manager"
    "mx_tree_widget"

    #"mx_codeview"
    #"mx_util_component"
    #"mx_context"
    #"mx_database"
    #"mx_font_manager"
    #"mx_projectexplorer"
    #"mx_referenceexplorer"
    #"mx_entityexplorer"
    #"mx_historywidget"
    #"mx_informationexplorer"
    #"mx_globalhighlighter"
    #"mx_macroexplorer"
    #"mx_codeviewtheme"
    #"mx_theme_manager"
    #"mx_generators"
    #"mx_action_manager"
    #"mx_mainwindowplugin"
    #"mx_callhierarchyplugin"
    #"$<$<BOOL:MX_ENABLE_PYTHON_BINDINGS>:mx_pythonconsole>"
)

target_include_directories("Multiplier" PRIVATE
  "include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_include_directories("Multiplier" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("Multiplier")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set_target_properties("Multiplier"
    PROPERTIES
      OUTPUT_NAME "multiplier"
  )

else()
  enable_appbundle_postbuild_steps("Multiplier")
endif()

if(MXQT_ENABLE_INSTALL)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(install_destination "bin")
  else()
    set(install_destination ".")
  endif()

  install(
    TARGETS "Multiplier"
    DESTINATION "${install_destination}"
  )
endif()
