#
# Copyright (c) 2024-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_agent_manager"
  include/multiplier/GUI/Managers/AgentManager.h
  include/multiplier/GUI/Managers/AgentMessage.h

  src/AgentTool.cpp
  src/AgentTool.h
  src/AgentToolRegistry.cpp
  src/AgentToolRegistry.h
  src/AgentSession.cpp
  src/AgentSession.h
  src/AgentManager.cpp

  src/tools/SpreadsheetTools.h
  src/tools/SpreadsheetTools.cpp

  src/tools/TaskTools.h
  src/tools/TaskTools.cpp

  src/tools/DocumentTools.h
  src/tools/DocumentTools.cpp

  src/tools/NavigationTools.h
  src/tools/NavigationTools.cpp

  src/tools/SessionTools.h
  src/tools/SessionTools.cpp

  src/tools/ObserverTools.h
  src/tools/ObserverTools.cpp
)

target_link_libraries("mx_agent_manager"
  PRIVATE
    "mx_cxx_flags"

  PUBLIC
    "mx_qt_library"
    "mx_interfaces"
    "mx_llm_manager"
    "mx_config_manager"
    "mx_spreadsheet_widget"
    "mx_multiplier_library"
)

target_include_directories("mx_agent_manager" PRIVATE
  "include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_include_directories("mx_agent_manager" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_agent_manager")
