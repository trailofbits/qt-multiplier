#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

project("xxhash" LANGUAGES C)

set(library_root "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_library("thirdparty_xxhash"
  "${library_root}/xxhash.h"
  "${library_root}/xxhash.c"
)

target_link_libraries("thirdparty_xxhash" PRIVATE
  "qtmx_common_flags"
)

target_include_directories("thirdparty_xxhash" PRIVATE
  "${library_root}"
)

target_include_directories("thirdparty_xxhash" SYSTEM INTERFACE
  "${library_root}"
)

target_compile_definitions("thirdparty_xxhash" PUBLIC
  "XXH_NAMESPACE=QTMULTIPLIER_"
)

add_library("xxhash::xxhash" ALIAS "thirdparty_xxhash")
