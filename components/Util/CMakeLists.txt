#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_util"
  include/multiplier/Util.h
  src/Util.h
  src/Util.cpp
)

target_link_libraries("mx_util" PRIVATE
  "qtmx_cxx_flags"
  "thirdparty_qt"
  "multiplier::mx-api"
  "mx_codetheme"
)

target_include_directories("mx_util" PRIVATE
  "include"
)

target_include_directories("mx_util" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_util")
