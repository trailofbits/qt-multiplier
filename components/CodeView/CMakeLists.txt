#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("codeview"
  include/multiplier/ui/ICodeView2.h
  src/ICodeView2.cpp

  src/CodeView2.h
  src/CodeView2.cpp

  include/multiplier/ui/ICodeModel.h
  src/ICodeModel.cpp

  src/CodeModel.h
  src/CodeModel.cpp
)

target_link_libraries("codeview" PRIVATE
  "qtmx_cxx_flags"
  "thirdparty_qt"
  "multiplier::mx-api"
  "mx_util"
  "mx_codetheme"
)

target_include_directories("codeview" PRIVATE
  "include"
)

target_include_directories("codeview" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("codeview")
