#
# Copyright (c) 2023-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(extra_sources
    src/MacosUtils.h
    src/MacosUtils.mm
  )
endif()

add_library("mx_builtin_theme"
  include/multiplier/GUI/Themes/BuiltinTheme.h
  src/DarkTheme.cpp
  src/IBuiltinTheme.cpp
  src/IBuiltinTheme.h
  src/LightTheme.cpp
  ${extra_sources}
)

target_link_libraries("mx_builtin_theme"
  PRIVATE
    "mx_cxx_flags"
    "mx_multiplier_library"
    "mx_media_manager"

  PUBLIC
    "mx_qt_library"
    "mx_interfaces"
)

target_include_directories("mx_builtin_theme" PRIVATE
  "include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_include_directories("mx_builtin_theme" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_builtin_theme")
