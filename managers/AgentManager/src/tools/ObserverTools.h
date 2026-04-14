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

class AgentManager;

struct ObserverToolContext {
  ConfigManager *config{nullptr};
  AgentManager *agent_manager{nullptr};  // for accessing primary session
  int64_t primary_session_id{-1};
};

// Register all observer tools with the given registry.
void registerObserverTools(AgentToolRegistry &registry,
                           ObserverToolContext *ctx);

// ---------------------------------------------------------------------------
// Individual tool classes
// ---------------------------------------------------------------------------

class GetPrimarySessionContextTool Q_DECL_FINAL : public AgentTool {
  ObserverToolContext *m_ctx;
 public:
  explicit GetPrimarySessionContextTool(ObserverToolContext *ctx)
      : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

class ObserverRecommendationTool Q_DECL_FINAL : public AgentTool {
  ObserverToolContext *m_ctx;
 public:
  explicit ObserverRecommendationTool(ObserverToolContext *ctx)
      : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

}  // namespace mx::gui
