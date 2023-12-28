#
# Copyright (c) 2023-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_call_hierarchy_plugin"
  include/multiplier/GUI/CallHierarchyPlugin.h

  src/CallHierarchyGenerator.cpp
  src/CallHierarchyGenerator.h
  src/CallHierarchyPlugin.cpp
)

target_link_libraries("mx_call_hierarchy_plugin"
  PRIVATE
    "mx_actionregistry"
    "mx_context"
    "mx_cxx_flags"
    "mx_generators"
    "mx_referenceexplorer"
    "mx_thememanager"
    "mx_util"

    "thirdparty_qt"
    "thirdparty_multiplier"
)

target_include_directories("mx_call_hierarchy_plugin" PRIVATE
  "include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_include_directories("mx_call_hierarchy_plugin" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_call_hierarchy_plugin")
