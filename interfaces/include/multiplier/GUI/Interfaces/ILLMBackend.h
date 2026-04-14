// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVector>

namespace mx::gui {

// A single message in a conversation.
struct LLMMessage {
  QString role;     // "system", "user", "assistant", "tool"
  QString content;

  // For tool result messages.
  QString tool_call_id;

  // For assistant messages that contain tool calls (serialized JSON).
  QString tool_calls_json;
};

// A tool call requested by the LLM.
struct ToolCall {
  QString id;
  QString name;
  QJsonObject arguments;
};

// Definition of a tool that the LLM can invoke.
struct ToolDefinition {
  QString name;
  QString description;
  QJsonObject parameters_schema;  // JSON Schema for the arguments.
};

// The response from an LLM backend.
struct LLMResponse {
  QString content;                   // Text response.
  QVector<ToolCall> tool_calls;      // Tool invocations requested.
  int prompt_tokens{0};
  int completion_tokens{0};
  QString stop_reason;
  QString error;                     // Non-empty on failure.
};

// Configuration passed to sendMessage.
struct LLMConfig {
  double temperature{0.0};
  int max_tokens{8192};
  QString model;
};

// Abstract interface for LLM backends. Implementations must be thread-safe
// for sendMessage (called from worker thread).
class ILLMBackend : public QObject {
  Q_OBJECT

 public:
  using QObject::QObject;
  virtual ~ILLMBackend(void) = default;

  // Human-readable backend name (e.g. "Claude", "OpenAI").
  virtual QString name(void) const = 0;

  // Send a message and block until the response is available.
  // This is called from a worker thread.
  virtual LLMResponse sendMessage(
      const QVector<LLMMessage> &messages,
      const QVector<ToolDefinition> &tools,
      const LLMConfig &config) = 0;

  // Check whether the backend has valid credentials configured.
  virtual bool validateCredentials(void) = 0;

 signals:
  // Emitted during streaming for progressive UI updates.
  void streamToken(const QString &token);

  // Emitted when a request fails.
  void requestFailed(const QString &error);
};

}  // namespace mx::gui
