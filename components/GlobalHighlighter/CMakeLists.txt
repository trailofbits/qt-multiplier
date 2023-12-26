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

  src/GlobalHighlighterItem.h
  src/GlobalHighlighterItem.cpp
)

target_sources("mx_globalhighlighter" INTERFACE
  Resources/Resources.qrc
)

target_link_libraries("mx_globalhighlighter"
  PRIVATE
    "mx_cxxflags"

  PUBLIC
    "thirdparty_qt"
    "thirdparty_multiplier"
    "mx_database"
    "mx_util"
    "mx_tokenlabel"
)

target_include_directories("mx_globalhighlighter" PRIVATE
  "include"
)

target_include_directories("mx_globalhighlighter" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_globalhighlighter")
