#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_executable("mx_projectexplorer_tests"
  src/main.cpp
  src/FileTreeModel.cpp
)

target_link_libraries("mx_projectexplorer_tests"
  PRIVATE
    "mx_cxx_flags"
    "mx_qt_library"
    "thirdparty_doctest"
    "mx_multiplier_library"
    "mx_projectexplorer"
)

target_include_directories("mx_projectexplorer_tests"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../src"
)

enable_qt_properties("mx_projectexplorer_tests")

add_test(
  NAME "projectexplorer"
  COMMAND "mx_projectexplorer_tests"
)
