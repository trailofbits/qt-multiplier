#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_reference_explorer_plugin"
  include/multiplier/GUI/IReferenceExplorerPlugin.h
  include/multiplier/GUI/ReferenceExplorer.h

  src/IReferenceExplorerPlugin.cpp

  src/ReferenceExplorerItemDelegate.h
  src/ReferenceExplorerItemDelegate.cpp

  src/ReferenceExplorerView.h
  src/ReferenceExplorerView.cpp

  src/ReferenceExplorer.h
  src/ReferenceExplorer.cpp

  src/ReferenceExplorerPlugin.h
  src/ReferenceExplorerPlugin.cpp
)

target_sources("mx_reference_explorer_plugin" INTERFACE
  Resources/Resources.qrc
)

target_link_libraries("mx_reference_explorer_plugin"
  PRIVATE
    "mx_cxx_flags"
    "mx_codeview"
    "mx_searchwidget"
    "mx_util_component"
    "mx_context"
    "mx_action_manager"
    "mx_mainwindowplugin"

    "mx_qt_library"
    "mx_multiplier_library"

    "gap::gap"

  PUBLIC
    "mx_globalhighlighter"
    "mx_theme_manager"
    "mx_generators"
)

target_include_directories("mx_reference_explorer_plugin" PRIVATE
  "include"
)

target_include_directories("mx_referenceexplorer" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_referenceexplorer")
