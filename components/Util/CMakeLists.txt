#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_util_component"
  include/multiplier/GUI/IModel.h
  src/IModel.cpp

  include/multiplier/GUI/Util.h
  src/Util.h
  src/Util.cpp

  # include/multiplier/GUI/TokenPainter.h
  # src/TokenPainter.cpp


  include/multiplier/GUI/Assert.h
  src/Assert.cpp
)

target_link_libraries("mx_util_component"
  PRIVATE
    "mx_cxx_flags"
    "mx_codeviewtheme"
    "mx_qt_library"
    "mx_multiplier_library"

  PUBLIC
    "mx_theme_manager"
)

target_include_directories("mx_util_component" PRIVATE
  "include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_include_directories("mx_util_component" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_util_component")
