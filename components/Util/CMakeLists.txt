#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_util"
  include/multiplier/ui/Util.h
  src/Util.h
  src/Util.cpp

  include/multiplier/ui/TokenPainter.h
  src/TokenPainter.cpp

  include/multiplier/ui/Icons.h
  src/Icons.cpp
)

target_link_libraries("mx_util"
  PRIVATE
    "qtmx_cxx_flags"
    "mx_codeviewtheme"
    "thirdparty_qt"
    "multiplier::mx-api"

  PUBLIC
    "mx_thememanager"
)

target_include_directories("mx_util" PRIVATE
  "include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_include_directories("mx_util" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_util")
