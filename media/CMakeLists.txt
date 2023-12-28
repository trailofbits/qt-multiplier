#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_media"
  include/multiplier/GUI/Media.h
  src/Media.cpp
)

target_sources("mx_media" INTERFACE
  resources/Resources.qrc
)

target_link_libraries("mx_media"
  PRIVATE
    "mx_cxx_flags"

  PUBLIC
    "thirdparty_qt"
)

target_include_directories("mx_media" PRIVATE
  "include"
)

target_include_directories("mx_media" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_media")
