#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_data_generators"
  include/multiplier/ui/CallHierarchyGenerator.h
  src/CallHierarchyGenerator.cpp
)

enable_qt_properties("mx_data_generators")

target_link_libraries("mx_data_generators"
  PRIVATE
    "qtmx_cxx_flags"

  PUBLIC
    "multiplier::mx-api"
    "mx_treeexplorer"
    "mx_generators"
)

target_include_directories("mx_data_generators" PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
