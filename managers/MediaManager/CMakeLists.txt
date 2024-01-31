#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_media_manager"
  include/multiplier/GUI/Managers/MediaManager.h
  src/MediaManager.cpp
)

target_sources("mx_media_manager" INTERFACE
  resources/Resources.qrc
)

target_link_libraries("mx_media_manager"
  PRIVATE
    "mx_cxx_flags"

  PUBLIC
    "mx_qt_library"
    "mx_theme_manager"
)

target_include_directories("mx_media_manager" PRIVATE
  "include"
)

target_include_directories("mx_media_manager" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_media_manager")

if(MXQT_ENABLE_INSTALL)
  install(
    FILES
      "${CMAKE_CURRENT_SOURCE_DIR}/resources/logo.png"

    DESTINATION
      "share/icons"
  )
endif()
