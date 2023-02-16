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

  src/ReferenceExplorer.h
  src/ReferenceExplorer.cpp

  src/ReferenceExplorerModel.h
  src/ReferenceExplorerModel.cpp

  src/ReferenceExplorerItemDelegate.h
  src/ReferenceExplorerItemDelegate.cpp

  src/Types.h

  src/NodeImporter.h
  src/NodeImporter.cpp
)

target_link_libraries("mx_referenceexplorer"
  PRIVATE
    "qtmx_cxx_flags"

  PUBLIC
    "thirdparty_qt"
    "multiplier::mx-api"
    "mx_assert"
)

target_include_directories("mx_referenceexplorer" PRIVATE
  "include"
)

target_include_directories("mx_referenceexplorer" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_referenceexplorer")
