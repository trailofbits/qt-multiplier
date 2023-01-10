#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

cmake_minimum_required(VERSION 3.19)

include("cmake/ccache.cmake")

project(qt-multiplier CXX)

include("cmake/system.cmake")
include("cmake/utils.cmake")
include("cmake/options.cmake")
include("cmake/build_flags.cmake")

if(PLATFORM_MACOS)
  include("cmake/macdeployqt.cmake")
endif()

find_package(Intl REQUIRED)

add_subdirectory("libraries")
add_subdirectory("components")

if(PLATFORM_MACOS)
  set(CMAKE_INSTALL_RPATH "@executable_path/../${CMAKE_INSTALL_LIBDIR}")
elseif(PLATFORM_LINUX)
  set(CMAKE_INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
endif()

add_library("mx-gui" STATIC
  Resources/resources.qrc
  Multiplier.cpp
  Multiplier.h
  MacOSUtils.h
  MacOSUtils.mm
)

target_link_libraries("mx-gui"
  PUBLIC
    multiplier::mx-api
    thirdparty_qt
    mx_codeview
    mx_util
    mx_database
)

enable_qt_properties("mx-gui")

add_executable("mx-open-file"
  OpenFile.cpp
)

target_link_libraries("mx-open-file"
  PRIVATE
    mx-gui
)

enable_qt_properties("mx-open-file")
