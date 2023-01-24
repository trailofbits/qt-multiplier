#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_filetreemodel"
  src/FileTreeModel.h
  src/FileTreeModel.cpp

  include/multiplier/ui/IFileTreeModel.h
  src/IFileTreeModel.cpp
)

target_link_libraries("mx_filetreemodel" PRIVATE
  "qtmx_cxx_flags"
  "thirdparty_qt"
  "multiplier::mx-api"
)

target_include_directories("mx_filetreemodel" PRIVATE
  "include"
)

target_include_directories("mx_filetreemodel" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_filetreemodel")

if(MXQT_ENABLE_TESTS)
  add_subdirectory("tests")
endif()
