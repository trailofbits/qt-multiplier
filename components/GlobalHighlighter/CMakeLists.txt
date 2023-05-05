#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_globalhighlighter"
  include/multiplier/ui/IGlobalHighlighter.h
  src/IGlobalHighlighter.cpp

  src/GlobalHighlighter.h
  src/GlobalHighlighter.cpp

  src/HighlightingModelProxy.h
  src/HighlightingModelProxy.cpp
)

target_link_libraries("mx_globalhighlighter"
  PRIVATE
    "qtmx_cxx_flags"

  PUBLIC
    "thirdparty_qt"
    "multiplier::mx-api"
    "mx_database"
)

target_include_directories("mx_globalhighlighter" PRIVATE
  "include"
)

target_include_directories("mx_globalhighlighter" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_globalhighlighter")
