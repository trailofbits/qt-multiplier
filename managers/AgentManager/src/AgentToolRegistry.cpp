// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "AgentToolRegistry.h"
#include "AgentTool.h"

namespace mx::gui {

void AgentToolRegistry::registerTool(std::unique_ptr<AgentTool> tool) {
  if (!tool) {
    return;
  }
  auto name = tool->name();
  m_tools[name] = std::move(tool);
}

AgentTool *AgentToolRegistry::tool(const QString &name) const {
  auto it = m_tools.find(name);
  if (it == m_tools.end()) {
    return nullptr;
  }
  return it->second.get();
}

QVector<ToolDefinition> AgentToolRegistry::allDefinitions(void) const {
  QVector<ToolDefinition> defs;
  defs.reserve(static_cast<int>(m_tools.size()));
  for (const auto &[_, tool] : m_tools) {
    defs.append(tool->toDefinition());
  }
  return defs;
}

}  // namespace mx::gui
