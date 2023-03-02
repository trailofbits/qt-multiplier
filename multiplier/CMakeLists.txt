#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

cmake_minimum_required(VERSION 3.19)

set(MACOSX_BUNDLE_ICON_FILE
  "Multiplier.icns"
)

set(multiplier_icon_path
  "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Icons/${MACOSX_BUNDLE_ICON_FILE}"
)

set_source_files_properties("${multiplier_icon_path}" PROPERTIES
  MACOSX_PACKAGE_LOCATION "Resources"
)

add_executable("Multiplier" MACOSX_BUNDLE
  src/main.cpp

  src/MainWindow.h
  src/MainWindow.cpp

  src/meta_types.h
  src/meta_types.cpp
)

target_sources("Multiplier" PRIVATE
  Resources/resources.qrc
  Info.plist
  "${multiplier_icon_path}"
)

add_subdirectory("widgets")

target_link_libraries("Multiplier"
  PRIVATE
    "qtmx_cxx_flags"

  PUBLIC
    "multiplier::mx-api"
    "thirdparty_qt"
    "mx_codeview"
    "mx_util"
    "mx_database"
    "mx_fontdatabase"
    "mx_indexview"
    "mx_referenceexplorer"
    "mx_widgets"
)

enable_qt_properties("Multiplier")
enable_appbundle_postbuild_steps("Multiplier")
