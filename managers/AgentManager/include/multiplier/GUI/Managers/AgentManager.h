// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Managers/AgentMessage.h>

#include <QObject>
#include <QString>
#include <QVector>

#include <memory>

namespace mx::gui {

class AgentTool;
class LLMManager;
struct LLMConfig;
class AgentManagerImpl;

class AgentManager Q_DECL_FINAL : public QObject {
  Q_OBJECT

 public:
  ~AgentManager(void);
  explicit AgentManager(LLMManager &llm_manager, QObject *parent = nullptr);

  // Session lifecycle.
  int64_t createSession(const QString &name, const QString &system_prompt,
                        const QString &backend_name = {});
  void sendMessage(int64_t session_id, const QString &text);
  void pauseSession(int64_t session_id);
  void resumeSession(int64_t session_id);
  void cancelSession(int64_t session_id);

  // Query.
  QVector<AgentMessage> sessionMessages(int64_t session_id) const;
  bool isSessionRunning(int64_t session_id) const;

  // Configuration.
  void setMaxIterations(int max);
  void setLLMConfig(const LLMConfig &config);

  // Register all built-in tools (spreadsheet, document, navigation, session).
  void registerBuiltinTools(class ConfigManager &config_manager);

  // Tool registry access (for explorers to register tools).
  void registerTool(std::unique_ptr<AgentTool> tool);

 signals:
  void messageAdded(int64_t session_id, const mx::gui::AgentMessage &msg);
  void toolCallStarted(int64_t session_id, const QString &name,
                       const QJsonObject &args);
  void toolCallCompleted(int64_t session_id, const QString &name,
                         const QJsonObject &result, int duration_ms);
  void sessionStarted(int64_t session_id);
  void sessionPaused(int64_t session_id);
  void sessionResumed(int64_t session_id);
  void sessionCompleted(int64_t session_id, const QString &summary);
  void sessionError(int64_t session_id, const QString &error);
  void tokenUsageUpdated(int64_t session_id, int prompt_tokens,
                         int completion_tokens);

 private:
  std::unique_ptr<AgentManagerImpl> d;
};

}  // namespace mx::gui
