// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Managers/ConfigManager.h>

#include "AgentTool.h"
#include "AgentToolRegistry.h"

namespace mx::gui {

struct PythonToolContext {
  ConfigManager *config{nullptr};
};

// Register all Python tools with the given registry.
void registerPythonTools(AgentToolRegistry &registry, PythonToolContext *ctx);

// ---------------------------------------------------------------------------
// Individual tool classes
// ---------------------------------------------------------------------------

class RunPythonTool Q_DECL_FINAL : public AgentTool {
  PythonToolContext *m_ctx;
 public:
  explicit RunPythonTool(PythonToolContext *ctx) : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

class CreateScriptFileTool Q_DECL_FINAL : public AgentTool {
  PythonToolContext *m_ctx;
 public:
  explicit CreateScriptFileTool(PythonToolContext *ctx) : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

class GetPythonApiReferenceTool Q_DECL_FINAL : public AgentTool {
  [[maybe_unused]] PythonToolContext *m_ctx;
 public:
  explicit GetPythonApiReferenceTool(PythonToolContext *ctx) : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

}  // namespace mx::gui
