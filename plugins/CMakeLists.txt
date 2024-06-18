#
# Copyright (c) 2024-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_plugins"
  include/multiplier/GUI/Plugins/BuiltinEntityInformationPlugin.h
  include/multiplier/GUI/Plugins/CallHierarchyPlugin.h
  include/multiplier/GUI/Plugins/ClassHierarchyPlugin.h
  include/multiplier/GUI/Plugins/StructExplorerPlugin.h

  src/BuiltinEntityInformationPlugin.cpp
  src/CallHierarchyPlugin.cpp
  src/ClassHierarchyPlugin.cpp
  src/StructExplorer.cpp
)

target_link_libraries("mx_plugins"
  PRIVATE
    "mx_action_manager"
    "mx_config_manager"
    "mx_cxx_flags"
    "mx_explorers"
    "mx_generator_widget"
    "mx_util_component"

  PUBLIC
    "mx_gap_library"
    "mx_multiplier_library"
    "mx_qt_library"
)

target_include_directories("mx_plugins" PRIVATE
  "include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_include_directories("mx_plugins" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_plugins")
