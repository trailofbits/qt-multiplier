#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

set(library_root "${CMAKE_CURRENT_SOURCE_DIR}/src/src")

# Note: it is important that this library is always built
# as a shared object due to licensing requirements
add_library("phantomstyle" SHARED
  "${library_root}/phantom/phantomtweak.h"

  "${library_root}/phantom/phantomcolor.h"
  "${library_root}/phantom/phantomcolor.cpp"

  "${library_root}/phantom/phantomstyle.h"
  "${library_root}/phantom/phantomstyle.cpp"
)

target_compile_options("phantomstyle" PRIVATE
  -Wno-deprecated-declarations
)

target_link_libraries("phantomstyle" PRIVATE
  ${MX_QT}::Core
  ${MX_QT}::Widgets
)

set_target_properties("phantomstyle"
  PROPERTIES
    AUTOMOC TRUE
    AUTOUIC TRUE
    AUTORCC TRUE
)

target_include_directories("phantomstyle" INTERFACE
  "${library_root}/phantom"
)
