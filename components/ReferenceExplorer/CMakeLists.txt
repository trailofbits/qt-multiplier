#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_referenceexplorer" STATIC
  include/multiplier/ui/IReferenceExplorerPlugin.h
  include/multiplier/ui/ReferenceExplorer.h

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

target_sources("mx_referenceexplorer" INTERFACE
  Resources/Resources.qrc
)

target_link_libraries("mx_referenceexplorer"
  PRIVATE
    "mx_cxxflags"
    "mx_codeview"
    "mx_searchwidget"
    "mx_util"
    "mx_actionregistry"
    "mx_mainwindowplugin"

    "thirdparty_qt"
    "thirdparty_multiplier"

    "gap::gap"

  PUBLIC
    "mx_globalhighlighter"
    "mx_thememanager"
    "mx_generators"
)

target_include_directories("mx_referenceexplorer" PRIVATE
  "include"
)

target_include_directories("mx_referenceexplorer" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_referenceexplorer")
