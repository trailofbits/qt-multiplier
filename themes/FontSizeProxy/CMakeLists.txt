#
# Copyright (c) 2023-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_font_size_proxy"
  include/multiplier/GUI/Themes/FontSizeProxy.h
  src/FontSizeProxy.cpp
)

target_link_libraries("mx_font_size_proxy"
  PRIVATE
    "mx_cxx_flags"

  PUBLIC
    "mx_qt_library"
    "mx_interfaces"
)

target_include_directories("mx_font_size_proxy" PRIVATE
  "include"
)

target_include_directories("mx_font_size_proxy" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_font_size_proxy")
