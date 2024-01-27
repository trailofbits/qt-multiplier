#
# Copyright (c) 2024-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(extra_sources
    src/MacosUtils.h
    src/MacosUtils.mm
  )
endif()

add_library("mx_code_widget"
  include/multiplier/GUI/Widgets/CodeWidget.h
  src/CodeWidget.cpp
  src/GoToLineWidget.cpp
  src/GoToLineWidget.h
  ${extra_sources}
)

target_link_libraries("mx_code_widget"
  PRIVATE
    "mx_action_manager"
    "mx_config_manager"
    "mx_cxx_flags"
    "mx_interfaces"
    "mx_media_manager"
    "mx_search_widget"
    "mx_theme_manager"
    "mx_util_component"

  PUBLIC
    "mx_multiplier_library"
    "mx_qt_library"
)

target_include_directories("mx_code_widget" PRIVATE
  "include"
)

target_include_directories("mx_code_widget" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_code_widget")
