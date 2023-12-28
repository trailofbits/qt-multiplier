#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_project_explorer_plugin"
  include/multiplier/GUI/IProjectExplorer.h
  src/IProjectExplorer.cpp

  include/multiplier/GUI/IFileTreeModel.h
  src/IFileTreeModel.cpp

  src/ProjectExplorer.h
  src/ProjectExplorer.cpp

  src/FileTreeModel.h
  src/FileTreeModel.cpp  
)

target_link_libraries("mx_project_explorer_plugin"
  PRIVATE
    "mx_cxx_flags"

  PUBLIC
    "thirdparty_qt"
    "thirdparty_multiplier"
    "mx_searchwidget"
)

target_include_directories("mx_project_explorer_plugin" PRIVATE
  "include"
)

target_include_directories("mx_project_explorer_plugin" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_project_explorer_plugin")

if(MXQT_ENABLE_TESTS)
  add_subdirectory("tests")
endif()
