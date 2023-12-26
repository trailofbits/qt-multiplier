#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_codeview"
  include/multiplier/ui/ICodeView.h
  src/ICodeView.cpp

  src/CodeView.h
  src/CodeView.cpp

  include/multiplier/ui/ICodeModel.h
  src/ICodeModel.cpp

  src/CodeModel.h
  src/CodeModel.cpp

  src/GoToLineWidget.h
  src/GoToLineWidget.cpp

  include/multiplier/ui/CodeWidget.h
  src/CodeWidget.cpp
)

target_link_libraries("mx_codeview"
  PRIVATE
    "mx_cxxflags"

  PUBLIC
    "thirdparty_qt"
    "thirdparty_multiplier"
    "mx_util"
    "mx_database"
    "mx_searchwidget"
    "mx_codeviewtheme"
    "mx_globalhighlighter"
    "mx_macroexplorer"
)

target_include_directories("mx_codeview" PRIVATE
  "include"
)

target_include_directories("mx_codeview" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_codeview")
