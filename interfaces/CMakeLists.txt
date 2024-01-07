#
# Copyright (c) 2023-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_interfaces"
  include/multiplier/GUI/Interfaces/IAction.h
  include/multiplier/GUI/Interfaces/IGeneratedItem.h
  include/multiplier/GUI/Interfaces/IListGenerator.h
  include/multiplier/GUI/Interfaces/IModel.h
  include/multiplier/GUI/Interfaces/ITheme.h
  include/multiplier/GUI/Interfaces/IThemeProxy.h
  include/multiplier/GUI/Interfaces/ITreeGenerator.h
  include/multiplier/GUI/Interfaces/IWindowManager.h
  include/multiplier/GUI/Interfaces/IWindowWidget.h

  src/IAction.cpp
  src/IGeneratedItem.cpp
  src/IListGenerator.cpp
  src/IModel.cpp
  src/ITheme.cpp
  src/IThemeProxy.cpp
  src/ITreeGenerator.cpp
  src/IWindowManager.cpp
  src/IWindowWidget.cpp
)

target_link_libraries("mx_interfaces"
  PRIVATE
    "mx_cxx_flags"

  PUBLIC
    "mx_gap_library"
    "mx_multiplier_library"
    "mx_qt_library"
)

target_include_directories("mx_interfaces" PRIVATE
  "include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_include_directories("mx_interfaces" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_interfaces")
