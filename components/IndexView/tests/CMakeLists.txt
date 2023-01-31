#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_executable("mx_indexview_tests"
  src/main.cpp
  src/FileTreeModel.cpp
)

target_link_libraries("mx_indexview_tests" PRIVATE
  "qtmx_cxx_flags"
  "thirdparty_qt"
  "thirdparty_doctest"
  "multiplier::mx-api"
  "mx_indexview"
)

target_include_directories("mx_indexview_tests" PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../src"
)

enable_qt_properties("mx_indexview_tests")

add_test(
  NAME "IndexView"
  COMMAND "mx_indexview_tests"
)
