#
# Copyright (c) 2024-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

find_package(Qt6 REQUIRED COMPONENTS Network)

add_library("mx_llm_manager"
  include/multiplier/GUI/Managers/LLMManager.h

  src/ClaudeBackend.cpp
  src/ClaudeBackend.h
  src/OpenAICompatBackend.cpp
  src/OpenAICompatBackend.h
  src/BedrockBackend.cpp
  src/BedrockBackend.h
  src/LLMManager.cpp
)

target_link_libraries("mx_llm_manager"
  PRIVATE
    "mx_cxx_flags"
    "Qt6::Network"

  PUBLIC
    "mx_qt_library"
    "mx_interfaces"
)

target_include_directories("mx_llm_manager" PRIVATE
  "include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_include_directories("mx_llm_manager" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_llm_manager")
