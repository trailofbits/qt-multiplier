#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_fontdatabase"
  include/multiplier/ui/FontDatabase.h
  src/FontDatabase.cpp
)

target_sources("mx_fontdatabase" INTERFACE
  Resources/fontdatabase.qrc
)

target_link_libraries("mx_fontdatabase" PRIVATE
  "qtmx_cxx_flags"
  "thirdparty_qt"
  "mx_assert"
)

target_include_directories("mx_fontdatabase" PRIVATE
  "include"
)

target_include_directories("mx_fontdatabase" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_fontdatabase")
