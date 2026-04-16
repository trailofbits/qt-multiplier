// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Managers/AgentManager.h>
#include <multiplier/GUI/Managers/ConfigManager.h>

#include "AgentTool.h"
#include "AgentToolRegistry.h"

namespace mx::gui {

struct SessionToolContext {
  ConfigManager *config{nullptr};
  int64_t current_session_id{-1};  // Set by AgentManager per session.
};

// Register all session tools with the given registry.
void registerSessionTools(AgentToolRegistry &registry,
                          SessionToolContext *ctx);

// ---------------------------------------------------------------------------
// Individual tool classes
// ---------------------------------------------------------------------------

class GetAuditContextTool Q_DECL_FINAL : public AgentTool {
  SessionToolContext *m_ctx;
 public:
  explicit GetAuditContextTool(SessionToolContext *ctx) : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

class SaveCheckpointTool Q_DECL_FINAL : public AgentTool {
  SessionToolContext *m_ctx;
 public:
  explicit SaveCheckpointTool(SessionToolContext *ctx) : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

class LogObservationTool Q_DECL_FINAL : public AgentTool {
  SessionToolContext *m_ctx;
 public:
  explicit LogObservationTool(SessionToolContext *ctx) : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

class FinishTool Q_DECL_FINAL : public AgentTool {
  [[maybe_unused]] SessionToolContext *m_ctx;
 public:
  explicit FinishTool(SessionToolContext *ctx) : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;

  // Access the last finish result (set after execute).
  static SessionResult lastResult(void);
  static bool wasCalledAndReset(void);

 private:
  static SessionResult s_last_result;
  static bool s_was_called;
};

}  // namespace mx::gui
