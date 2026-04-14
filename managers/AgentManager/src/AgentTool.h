// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QJsonObject>
#include <QString>

#include <multiplier/GUI/Interfaces/ILLMBackend.h>

namespace mx::gui {

// Base class for agent tools. Subclass and register with AgentManager.
class AgentTool {
 public:
  virtual ~AgentTool(void) = default;

  virtual QString name(void) const = 0;
  virtual QString description(void) const = 0;
  virtual QJsonObject parametersSchema(void) const = 0;
  virtual QJsonObject execute(const QJsonObject &args) = 0;

  // Convert to ToolDefinition for the LLM backend.
  ToolDefinition toDefinition(void) const;
};

}  // namespace mx::gui
