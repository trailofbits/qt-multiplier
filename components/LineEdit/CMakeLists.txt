#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_lineedit"
  include/multiplier/ui/ILineEdit.h
  src/ILineEdit.cpp

  src/LineEdit.h
  src/LineEdit.cpp
)

target_link_libraries("mx_lineedit"
  PRIVATE
    "qtmx_cxx_flags"

  PUBLIC
    "thirdparty_qt"
)

target_include_directories("mx_lineedit" PRIVATE
  "include"
)

target_include_directories("mx_lineedit" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_lineedit")
