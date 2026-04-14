// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "AgentTool.h"

namespace mx::gui {

ToolDefinition AgentTool::toDefinition(void) const {
  ToolDefinition def;
  def.name = name();
  def.description = description();
  def.parameters_schema = parametersSchema();
  return def;
}

}  // namespace mx::gui
