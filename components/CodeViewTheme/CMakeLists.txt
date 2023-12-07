#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_codeviewtheme"
  include/multiplier/ui/CodeViewTheme.h
  src/CodeViewTheme.cpp

  src/DefaultCodeViewThemes.h
  src/DefaultCodeViewThemes.cpp
)

target_link_libraries("mx_codeviewtheme"
  PRIVATE
    "qtmx_cxx_flags"

  PUBLIC
    "thirdparty_qt"
    "multiplier::multiplier"
)

target_include_directories("mx_codeviewtheme" PRIVATE
  "include"
)

target_include_directories("mx_codeviewtheme" SYSTEM INTERFACE
  "include"
)
