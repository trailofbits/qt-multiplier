#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_widgets" STATIC
  src/PreviewableReferenceExplorer.h
  src/PreviewableReferenceExplorer.cpp

  src/QuickReferenceExplorer.h
  src/QuickReferenceExplorer.cpp

  src/SimpleTextInputDialog.h
  src/SimpleTextInputDialog.cpp

  src/MxTabWidget.h
  src/MxTabWidget.cpp

  src/InformationExplorerWidget.h
  src/InformationExplorerWidget.cpp
)

target_include_directories("mx_widgets" SYSTEM INTERFACE
  src
)

target_link_libraries("mx_widgets"
  PRIVATE
    "qtmx_cxx_flags"

  PUBLIC
    "thirdparty_qt"
    "multiplier::mx-api"
    "mx_codeview"
    "mx_treeexplorer"
    "mx_macroexplorer"
    "mx_globalhighlighter"
    "mx_informationexplorer"
)

enable_qt_properties("mx_widgets")
