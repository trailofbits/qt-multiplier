#
# Copyright (c) 2023-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_theme_manager"
  include/multiplier/GUI/Managers/ThemeManager.h
  src/ThemeManager.cpp
  src/ITheme.cpp
)

target_link_libraries("mx_theme_manager"
  PRIVATE
    "mx_cxx_flags"
    "mx_multiplier_library"

  PUBLIC
    "mx_qt_library"
)

target_include_directories("mx_theme_manager" PRIVATE
  "include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_include_directories("mx_theme_manager" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_theme_manager")
