#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_codetheme"
  include/multiplier/CodeTheme.h
  src/CodeTheme.h
  src/CodeTheme.cpp
)

target_link_libraries("mx_codetheme" PRIVATE
  "qtmx_cxx_flags"
  "thirdparty_qt"
  "multiplier::mx-api"
)

target_include_directories("mx_codetheme" PRIVATE
  "include"
)

target_include_directories("mx_codetheme" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_codetheme")
