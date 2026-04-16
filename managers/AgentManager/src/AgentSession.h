// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Interfaces/ILLMBackend.h>
#include <multiplier/GUI/Managers/AgentMessage.h>

#include <QAtomicInt>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QVector>

namespace mx::gui {

class AgentToolRegistry;
class ConfigManager;

class AgentSession Q_DECL_FINAL : public QObject {
  Q_OBJECT

 public:
  AgentSession(int64_t session_id, ILLMBackend *backend,
               AgentToolRegistry *tools, const LLMConfig &config,
               const QString &system_prompt, int max_iterations,
               ConfigManager *config_manager = nullptr,
               QObject *parent = nullptr);
  ~AgentSession(void) override;

  int64_t sessionId(void) const;
  int64_t rootNodeId(void) const;
  QVector<AgentMessage> messages(void) const;
  bool isRunning(void) const;

  // Load messages from an external source (e.g. DB) to restore history.
  void loadMessages(const QVector<AgentMessage> &messages);

  // Start processing a user message. Runs the agentic loop in a thread.
  void sendUserMessage(const QString &text);

  void pause(void);
  void resume(void);
  void cancel(void);

 signals:
  void messageAdded(const mx::gui::AgentMessage &msg);
  void toolCallStarted(const QString &name, const QJsonObject &args);
  void toolCallCompleted(const QString &name, const QJsonObject &result,
                         int duration_ms);
  void sessionStarted(void);
  void sessionPaused(void);
  void sessionResumed(void);
  void sessionCompleted(const QString &summary);
  void sessionFinished(const mx::gui::SessionResult &result);
  void sessionError(const QString &error);
  void tokenUsageUpdated(int prompt_tokens, int completion_tokens);

 private:
  // The agentic loop, run on a worker thread.
  void runLoop(void);

  // Convert the message history to LLMMessages for the backend.
  QVector<LLMMessage> buildMessages(void) const;

  // Add a message to the history and emit the signal.
  AgentMessage addMessage(const QString &role, const QString &content,
                          const QString &tool_name = {},
                          const QString &tool_call_id = {},
                          const QJsonObject &tool_args = {},
                          const QJsonObject &tool_result = {},
                          int token_count = 0,
                          int duration_ms = 0);

  int64_t m_session_id;
  ILLMBackend *m_backend;
  AgentToolRegistry *m_tools;
  ConfigManager *m_config_manager{nullptr};
  SessionResult m_pending_finish_result;
  LLMConfig m_config;
  QString m_system_prompt;
  int m_max_iterations;

  mutable QMutex m_mutex;
  QVector<AgentMessage> m_messages;
  int64_t m_next_message_id{0};

  QAtomicInt m_running{0};
  QAtomicInt m_paused{0};
  QAtomicInt m_cancelled{0};

  int m_total_prompt_tokens{0};
  int m_total_completion_tokens{0};

  // Cost tracking node IDs.
  int64_t m_root_node_id{-1};
  int64_t m_current_llm_node_id{-1};

  // Dependency edge tracking: tool_call_id string -> cost node_id.
  QMap<QString, int64_t> m_tool_call_id_to_node;

  // Tool nodes completed since the last LLM call, used for context edges.
  QVector<int64_t> m_pending_context_nodes;
};

}  // namespace mx::gui
