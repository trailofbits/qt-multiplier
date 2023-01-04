#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_assert"
  include/multiplier/ui/Assert.h
  src/Assert.cpp
)

target_link_libraries("mx_assert" PRIVATE
  "qtmx_cxx_flags"
  "thirdparty_qt"
)

target_include_directories("mx_assert" PRIVATE
  "include"
)

target_include_directories("mx_assert" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_assert")
