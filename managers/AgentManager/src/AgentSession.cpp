// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "AgentSession.h"
#include "AgentTool.h"
#include "AgentToolRegistry.h"

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QThread>

namespace mx::gui {

AgentSession::AgentSession(int64_t session_id, ILLMBackend *backend,
                           AgentToolRegistry *tools, const LLMConfig &config,
                           const QString &system_prompt, int max_iterations,
                           QObject *parent)
    : QObject(parent),
      m_session_id(session_id),
      m_backend(backend),
      m_tools(tools),
      m_config(config),
      m_system_prompt(system_prompt),
      m_max_iterations(max_iterations) {}

AgentSession::~AgentSession(void) = default;

int64_t AgentSession::sessionId(void) const {
  return m_session_id;
}

QVector<AgentMessage> AgentSession::messages(void) const {
  QMutexLocker lock(&m_mutex);
  return m_messages;
}

bool AgentSession::isRunning(void) const {
  return m_running.loadRelaxed() != 0;
}

void AgentSession::pause(void) {
  m_paused.storeRelaxed(1);
  emit sessionPaused();
}

void AgentSession::resume(void) {
  m_paused.storeRelaxed(0);
  emit sessionResumed();
}

void AgentSession::cancel(void) {
  m_cancelled.storeRelaxed(1);
}

AgentMessage AgentSession::addMessage(
    const QString &role, const QString &content, const QString &tool_name,
    const QString &tool_call_id, const QJsonObject &tool_args,
    const QJsonObject &tool_result, int token_count) {
  AgentMessage msg;
  msg.session_id = m_session_id;
  msg.role = role;
  msg.content = content;
  msg.tool_name = tool_name;
  msg.tool_call_id = tool_call_id;
  msg.tool_args = tool_args;
  msg.tool_result = tool_result;
  msg.timestamp = QDateTime::currentDateTime();
  msg.token_count = token_count;

  {
    QMutexLocker lock(&m_mutex);
    msg.message_id = m_next_message_id++;
    m_messages.append(msg);
  }

  emit messageAdded(msg);
  return msg;
}

QVector<LLMMessage> AgentSession::buildMessages(void) const {
  QMutexLocker lock(&m_mutex);

  QVector<LLMMessage> llm_messages;

  // System prompt first.
  if (!m_system_prompt.isEmpty()) {
    LLMMessage sys;
    sys.role = QStringLiteral("system");
    sys.content = m_system_prompt;
    llm_messages.append(sys);
  }

  for (const auto &msg : m_messages) {
    if (msg.role == QStringLiteral("user")) {
      LLMMessage m;
      m.role = QStringLiteral("user");
      m.content = msg.content;
      llm_messages.append(m);

    } else if (msg.role == QStringLiteral("assistant")) {
      LLMMessage m;
      m.role = QStringLiteral("assistant");
      m.content = msg.content;
      llm_messages.append(m);

    } else if (msg.role == QStringLiteral("tool_call")) {
      // Represent as an assistant message with tool_calls_json.
      // Check if we already appended an assistant message for this group.
      // Tool calls are serialized as individual messages but need to be
      // grouped as a single assistant message for the API.
      //
      // Build a tool_use block for Claude format.
      QJsonObject tool_use;
      tool_use[QStringLiteral("type")] = QStringLiteral("tool_use");
      tool_use[QStringLiteral("id")] = msg.tool_call_id;
      tool_use[QStringLiteral("name")] = msg.tool_name;
      tool_use[QStringLiteral("input")] = msg.tool_args;

      // Check if the last LLM message is already an assistant with tool calls.
      if (!llm_messages.isEmpty() &&
          llm_messages.last().role == QStringLiteral("assistant") &&
          !llm_messages.last().tool_calls_json.isEmpty()) {
        // Append to existing tool calls array.
        auto doc = QJsonDocument::fromJson(
            llm_messages.last().tool_calls_json.toUtf8());
        auto arr = doc.array();
        arr.append(tool_use);
        llm_messages.last().tool_calls_json =
            QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
      } else {
        LLMMessage m;
        m.role = QStringLiteral("assistant");
        QJsonArray arr;
        arr.append(tool_use);
        m.tool_calls_json = QString::fromUtf8(
            QJsonDocument(arr).toJson(QJsonDocument::Compact));
        llm_messages.append(m);
      }

    } else if (msg.role == QStringLiteral("tool_result")) {
      LLMMessage m;
      m.role = QStringLiteral("tool");
      m.content = msg.content;
      m.tool_call_id = msg.tool_call_id;
      llm_messages.append(m);
    }
  }

  return llm_messages;
}

void AgentSession::sendUserMessage(const QString &text) {
  if (m_running.loadRelaxed()) {
    return;
  }

  addMessage(QStringLiteral("user"), text);

  auto *thread = QThread::create([this] { runLoop(); });
  QObject::connect(thread, &QThread::finished, thread, &QThread::deleteLater);
  thread->start();
}

void AgentSession::runLoop(void) {
  m_running.storeRelaxed(1);
  m_cancelled.storeRelaxed(0);
  m_paused.storeRelaxed(0);

  emit sessionStarted();

  int iteration = 0;

  while (iteration < m_max_iterations) {
    // Check for cancellation.
    if (m_cancelled.loadRelaxed()) {
      m_running.storeRelaxed(0);
      emit sessionError(QStringLiteral("Session cancelled by user"));
      return;
    }

    // Wait while paused.
    while (m_paused.loadRelaxed()) {
      if (m_cancelled.loadRelaxed()) {
        m_running.storeRelaxed(0);
        emit sessionError(QStringLiteral("Session cancelled while paused"));
        return;
      }
      QThread::msleep(100);
    }

    auto llm_messages = buildMessages();
    auto tool_defs = m_tools->allDefinitions();

    auto response = m_backend->sendMessage(llm_messages, tool_defs, m_config);

    if (!response.error.isEmpty()) {
      m_running.storeRelaxed(0);
      emit sessionError(response.error);
      return;
    }

    m_total_prompt_tokens += response.prompt_tokens;
    m_total_completion_tokens += response.completion_tokens;
    emit tokenUsageUpdated(m_total_prompt_tokens, m_total_completion_tokens);

    // If there is text content, add an assistant message.
    if (!response.content.isEmpty()) {
      addMessage(QStringLiteral("assistant"), response.content,
                 {}, {}, {}, {}, response.completion_tokens);
    }

    // If there are no tool calls, we are done.
    if (response.tool_calls.isEmpty()) {
      m_running.storeRelaxed(0);
      emit sessionCompleted(response.content);
      return;
    }

    // Process each tool call.
    for (const auto &call : response.tool_calls) {
      if (m_cancelled.loadRelaxed()) {
        m_running.storeRelaxed(0);
        emit sessionError(QStringLiteral("Session cancelled during tool execution"));
        return;
      }

      // Record the tool call.
      addMessage(QStringLiteral("tool_call"), {},
                 call.name, call.id, call.arguments);
      emit toolCallStarted(call.name, call.arguments);

      QElapsedTimer timer;
      timer.start();

      QJsonObject result;
      auto *tool = m_tools->tool(call.name);
      if (tool) {
        result = tool->execute(call.arguments);
      } else {
        result[QStringLiteral("error")] =
            QStringLiteral("Unknown tool: %1").arg(call.name);
      }

      auto duration_ms = static_cast<int>(timer.elapsed());
      emit toolCallCompleted(call.name, result, duration_ms);

      // Record the tool result.
      auto result_str = QString::fromUtf8(
          QJsonDocument(result).toJson(QJsonDocument::Compact));
      addMessage(QStringLiteral("tool_result"), result_str,
                 call.name, call.id, {}, result);
    }

    ++iteration;
  }

  m_running.storeRelaxed(0);
  emit sessionError(QStringLiteral("Maximum iterations (%1) reached")
                        .arg(m_max_iterations));
}

}  // namespace mx::gui
