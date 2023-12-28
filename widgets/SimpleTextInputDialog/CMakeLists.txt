#
# Copyright (c) 2023-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_simple_text_input_dialog"
  include/multiplier/GUI/Widgets/SimpleTextInputDialog.h
  src/SimpleTextInputDialog.cpp
)

target_link_libraries("mx_simple_text_input_dialog"
  PRIVATE
    "mx_cxx_flags"

  PUBLIC
    "mx_qt_library"
)

target_include_directories("mx_simple_text_input_dialog" PRIVATE
  "include"
)

target_include_directories("mx_simple_text_input_dialog" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_simple_text_input_dialog")
