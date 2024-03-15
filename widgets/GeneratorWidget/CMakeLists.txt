#
# Copyright (c) 2023-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_generator_widget"
  include/multiplier/GUI/Widgets/ListGeneratorWidget.h
  include/multiplier/GUI/Widgets/TreeGeneratorWidget.h

  src/ListGeneratorWidget.cpp
  src/TreeGeneratorWidget.cpp

  src/ExpandTreeRunnable.cpp
  src/ExpandTreeRunnable.h

  src/IGenerateTreeRunnable.cpp
  src/IGenerateTreeRunnable.h

  src/InitTreeRunnable.cpp
  src/InitTreeRunnable.h

  src/ListGeneratorModel.cpp
  src/ListGeneratorModel.h

  src/SearchFilterModelProxy.cpp
  src/SearchFilterModelProxy.h

  src/TreeGeneratorModel.cpp
  src/TreeGeneratorModel.h
)

target_link_libraries("mx_generator_widget"
  PRIVATE
    "mx_config_manager"
    "mx_cxx_flags"
    "mx_filter_settings_widget"
    "mx_gap_library"
    "mx_media_manager"
    "mx_multiplier_library"
    "mx_search_widget"
    "mx_theme_manager"
    "mx_tree_widget"
    "mx_util_component"
    "mx_registry"

  PUBLIC
    "mx_interfaces"
    "mx_qt_library"
)

target_include_directories("mx_generator_widget" PRIVATE
  "include"
)

target_include_directories("mx_generator_widget" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_generator_widget")
