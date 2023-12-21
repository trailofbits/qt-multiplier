#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_referenceexplorer"
  src/ReferenceExplorerItemDelegate.h
  src/ReferenceExplorerItemDelegate.cpp

  src/ReferenceExplorerView.h
  src/ReferenceExplorerView.cpp

  include/multiplier/ui/ReferenceExplorer.h
  src/ReferenceExplorer.cpp
)

target_sources("mx_referenceexplorer" INTERFACE
  Resources/Resources.qrc
)

target_link_libraries("mx_referenceexplorer"
  PRIVATE
    "qtmx_cxx_flags"

  PUBLIC
    "thirdparty_qt"
    "thirdparty_multiplier"
    "gap::gap"
    "mx_searchwidget"
    "mx_util"
    "mx_codeview"
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
