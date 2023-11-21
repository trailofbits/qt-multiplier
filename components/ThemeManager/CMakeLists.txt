#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_thememanager"
  include/multiplier/ui/IThemeManager.h
  src/IThemeManager.cpp

  src/ThemeManager.h
  src/ThemeManager.cpp

  src/Theme.h
  src/Theme.cpp
)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  target_sources("mx_thememanager" PRIVATE
    src/macos_utils.h
    src/macos_utils.mm
  )
endif()

target_link_libraries("mx_thememanager"
  PRIVATE
    "qtmx_cxx_flags"
    "thirdparty_qt"
    "mx_util"

  PUBLIC
    "mx_codeviewtheme"
)

target_include_directories("mx_thememanager" PRIVATE
  "include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_include_directories("mx_thememanager" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_thememanager")
