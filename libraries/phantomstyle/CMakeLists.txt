#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

set(library_root
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

add_library("thirdparty_phantomstyle" STATIC
  "${library_root}/src/phantom/phantomtweak.h"

  "${library_root}/src/phantom/phantomcolor.h"
  "${library_root}/src/phantom/phantomcolor.cpp"

  "${library_root}/src/phantom/phantomstyle.h"
  "${library_root}/src/phantom/phantomstyle.cpp"
)

target_compile_definitions("thirdparty_phantomstyle" PUBLIC
  QT_VERSION=620
)

target_link_libraries("thirdparty_phantomstyle"
  PRIVATE
    "mx_cxx_flags"

  PUBLIC
    "mx_qt_library"
)

target_include_directories("thirdparty_phantomstyle" PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/patches"
)

target_include_directories("thirdparty_phantomstyle" SYSTEM INTERFACE
  "${library_root}/src"
)

set_source_files_properties(
  "${library_root}/src/phantom/phantomstyle.cpp"

  PROPERTIES
    COMPILE_OPTIONS
      "-include;qt6_compatibility.h"
)

enable_qt_properties("thirdparty_phantomstyle")

add_library("thirdparty_phantomstyle_disable_warnings" INTERFACE)
target_compile_options("thirdparty_phantomstyle_disable_warnings" INTERFACE
  -Wno-everything
  -Wno-error
)

target_link_libraries("thirdparty_phantomstyle"
  PRIVATE
    "thirdparty_phantomstyle_disable_warnings"
)
