#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_indexview"
  include/multiplier/ui/IIndexView.h
  src/IIndexView.cpp

  include/multiplier/ui/IFileTreeModel.h
  src/IFileTreeModel.cpp

  src/IndexView.h
  src/IndexView.cpp

  src/FileTreeModel.h
  src/FileTreeModel.cpp  
)

target_link_libraries("mx_indexview"
  PRIVATE
    "qtmx_cxx_flags"

  PUBLIC
    "thirdparty_qt"
    "multiplier::mx-api"
    "mx_searchwidget"
)

target_include_directories("mx_indexview" PRIVATE
  "include"
)

target_include_directories("mx_indexview" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_indexview")

if(MXQT_ENABLE_TESTS)
  add_subdirectory("tests")
endif()
