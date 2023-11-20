#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_executable("mx_treeexplorer_tests"
  src/main.cpp
)

target_link_libraries("mx_treeexplorer_tests"
  PRIVATE
    "qtmx_cxx_flags"
    "mx_treeexplorer"
    "thirdparty_doctest"
)

target_include_directories("mx_treeexplorer_tests" PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../src"
)

enable_qt_properties("mx_treeexplorer_tests")
