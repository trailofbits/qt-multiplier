#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_util_component"
  include/multiplier/GUI/Util.h
  src/Util.h
  src/Util.cpp
)

target_link_libraries("mx_util_component"
  PRIVATE
    "mx_cxx_flags"

  PUBLIC
    "mx_multiplier_library"
    "mx_qt_library"
    "mx_theme_manager"
)

target_include_directories("mx_util_component" PRIVATE
  "include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_include_directories("mx_util_component" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_util_component")
