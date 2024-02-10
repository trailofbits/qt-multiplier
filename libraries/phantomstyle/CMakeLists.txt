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

add_library("mx_phantomstyle_library" SHARED
  "${library_root}/src/phantom/phantomtweak.h"

  "${library_root}/src/phantom/phantomcolor.h"
  "${library_root}/src/phantom/phantomcolor.cpp"

  "${library_root}/src/phantom/phantomstyle.h"
  "${library_root}/src/phantom/phantomstyle.cpp"
)

target_compile_definitions("mx_phantomstyle_library" PUBLIC
  QT_VERSION=620
)

target_link_libraries("mx_phantomstyle_library"
  PRIVATE
    "mx_cxx_flags"

  PUBLIC
    "mx_qt_library"
)

target_include_directories("mx_phantomstyle_library" PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/patches"
)

target_include_directories("mx_phantomstyle_library" SYSTEM INTERFACE
  "${library_root}/src"
)

set_source_files_properties(
  "${library_root}/src/phantom/phantomstyle.cpp"

  PROPERTIES
    COMPILE_OPTIONS
      "-include;qt6_compatibility.h"
)

enable_qt_properties("mx_phantomstyle_library")

add_library("mx_phantomstyle_library_disable_warnings" INTERFACE)
target_compile_options("mx_phantomstyle_library_disable_warnings" INTERFACE
  -Wno-everything
  -Wno-error
)

target_link_libraries("mx_phantomstyle_library"
  PRIVATE
    "mx_phantomstyle_library_disable_warnings"
)

if(MXQT_ENABLE_INSTALL)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(install_destination "lib")
  else()
    set(install_destination ".")
  endif()

  set_target_properties("mx_phantomstyle_library" PROPERTIES
    INSTALL_RPATH "$ORIGIN/../lib"
  )

  install(
    TARGETS "mx_phantomstyle_library"
    DESTINATION "${install_destination}"
  )
endif()
