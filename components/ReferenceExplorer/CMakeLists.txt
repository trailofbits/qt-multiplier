#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_referenceexplorer"
  include/multiplier/ui/IReferenceExplorer.h
  src/IReferenceExplorer.cpp

  include/multiplier/ui/IReferenceExplorerModel.h
  src/IReferenceExplorerModel.cpp
  
  
  include/multiplier/ui/INodeGenerator.h
  src/INodeGenerator.cpp

  src/ReferenceExplorer.h
  src/ReferenceExplorer.cpp

  src/ReferenceExplorerModel.h
  src/ReferenceExplorerModel.cpp

  src/ReferenceExplorerItemDelegate.h
  src/ReferenceExplorerItemDelegate.cpp

  src/CallHierarchyChildGenerator.h
  src/CallHierarchyChildGenerator.cpp
  
  src/CallHierarchyRootGenerator.h
  src/CallHierarchyRootGenerator.cpp
  
  src/TaintedChildGenerator.h
  src/TaintedChildGenerator.cpp
  
  src/TaintedRootGenerator.h
  src/TaintedRootGenerator.cpp

  src/Utils.h
  src/Utils.cpp

  src/FilterSettingsWidget.h
  src/FilterSettingsWidget.cpp

  src/SearchFilterModelProxy.h
  src/SearchFilterModelProxy.cpp
)

target_sources("mx_referenceexplorer" INTERFACE
  Resources/Resources.qrc
)

target_link_libraries("mx_referenceexplorer"
  PRIVATE
    "qtmx_cxx_flags"

  PUBLIC
    "thirdparty_qt"
    "multiplier::mx-api"
    "multiplier::mx-analysis"
    "mx_assert"
    "mx_searchwidget"
    "mx_util"
)

target_include_directories("mx_referenceexplorer" PRIVATE
  "include"
)

target_include_directories("mx_referenceexplorer" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_referenceexplorer")
