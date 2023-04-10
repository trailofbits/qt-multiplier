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

  src/Theme.h
  src/Theme.cpp
  
  src/Style.h
  src/Style.cpp
)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  target_sources("Multiplier" PRIVATE
    src/macos_utils.h
    src/macos_utils.mm
  )
endif()

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
    "thirdparty_phantomstyle"
    "mx_codeview"
    "mx_util"
    "mx_database"
    "mx_fontdatabase"
    "mx_indexview"
    "mx_referenceexplorer"
    "mx_entityexplorer"
    "mx_widgets"
)

enable_qt_properties("Multiplier")
enable_appbundle_postbuild_steps("Multiplier")

if(MXQT_ENABLE_INSTALL)
  if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/opt/multiplier" CACHE PATH "" FORCE)
  endif()

  install(
    TARGETS "Multiplier"
    DESTINATION "."
  )
endif()
