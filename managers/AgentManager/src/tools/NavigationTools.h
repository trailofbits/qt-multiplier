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

struct NavigationToolContext {
  ConfigManager *config{nullptr};
};

// Register all navigation tools with the given registry.
void registerNavigationTools(AgentToolRegistry &registry,
                             NavigationToolContext *ctx);

// ---------------------------------------------------------------------------
// Individual tool classes
// ---------------------------------------------------------------------------

class SearchEntitiesTool Q_DECL_FINAL : public AgentTool {
  NavigationToolContext *m_ctx;
 public:
  explicit SearchEntitiesTool(NavigationToolContext *ctx) : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

class GetDefinitionTool Q_DECL_FINAL : public AgentTool {
  NavigationToolContext *m_ctx;
 public:
  explicit GetDefinitionTool(NavigationToolContext *ctx) : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

class GetReferencesTool Q_DECL_FINAL : public AgentTool {
  NavigationToolContext *m_ctx;
 public:
  explicit GetReferencesTool(NavigationToolContext *ctx) : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

class ListFilesTool Q_DECL_FINAL : public AgentTool {
  NavigationToolContext *m_ctx;
 public:
  explicit ListFilesTool(NavigationToolContext *ctx) : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

}  // namespace mx::gui
