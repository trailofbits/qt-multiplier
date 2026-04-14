// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Interfaces/ILLMBackend.h>

#include <memory>
#include <unordered_map>
#include <vector>

namespace mx::gui {

class AgentTool;

struct QStringHash {
  size_t operator()(const QString &s) const { return qHash(s); }
};

class AgentToolRegistry {
 public:
  AgentToolRegistry(void) = default;
  ~AgentToolRegistry(void) = default;

  // Register a tool. Ownership is transferred.
  void registerTool(std::unique_ptr<AgentTool> tool);

  // Look up a tool by name. Returns nullptr if not found.
  AgentTool *tool(const QString &name) const;

  // All tool definitions for passing to the LLM backend.
  QVector<ToolDefinition> allDefinitions(void) const;

 private:
  std::unordered_map<QString, std::unique_ptr<AgentTool>, QStringHash>
      m_tools;
};

}  // namespace mx::gui
