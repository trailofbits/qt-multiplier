#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_global_highlighter_plugin"
  include/multiplier/GUI/IGlobalHighlighter.h
  src/IGlobalHighlighter.cpp

  src/GlobalHighlighter.h
  src/GlobalHighlighter.cpp

  src/HighlightingModelProxy.h
  src/HighlightingModelProxy.cpp

  src/GlobalHighlighterItem.h
  src/GlobalHighlighterItem.cpp
)

target_sources("mx_global_highlighter_plugin" INTERFACE
  Resources/Resources.qrc
)

target_link_libraries("mx_global_highlighter_plugin"
  PRIVATE
    "mx_cxx_flags"

  PUBLIC
    "mx_qt_library"
    "mx_multiplier_library"
    "mx_database"
    "mx_util_component"
    "mx_tokenlabel"
)

target_include_directories("mx_global_highlighter_plugin" PRIVATE
  "include"
)

target_include_directories("mx_global_highlighter_plugin" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_global_highlighter_plugin")
