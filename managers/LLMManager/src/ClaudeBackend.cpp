// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "ClaudeBackend.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace mx::gui {

static const QString kApiUrl =
    QStringLiteral("https://api.anthropic.com/v1/messages");
static const QString kDefaultModel =
    QStringLiteral("claude-sonnet-4-20250514");

ClaudeBackend::ClaudeBackend(QObject *parent)
    : ILLMBackend(parent),
      m_network(new QNetworkAccessManager(this)) {}

ClaudeBackend::~ClaudeBackend(void) = default;

QString ClaudeBackend::name(void) const {
  return QStringLiteral("Claude");
}

void ClaudeBackend::setConfig(const QString &key, const QString &value) {
  m_config[key] = value;
}

QString ClaudeBackend::config(const QString &key) const {
  return m_config.value(key);
}

bool ClaudeBackend::validateCredentials(void) {
  return !m_config.value(QStringLiteral("api_key")).isEmpty();
}

QJsonObject ClaudeBackend::buildRequestBody(
    const QVector<LLMMessage> &messages,
    const QVector<ToolDefinition> &tools,
    const LLMConfig &config) const {

  QJsonObject body;

  QString model = config.model;
  if (model.isEmpty()) {
    model = m_config.value(QStringLiteral("model"), kDefaultModel);
  }
  body[QStringLiteral("model")] = model;
  body[QStringLiteral("max_tokens")] = config.max_tokens;
  body[QStringLiteral("temperature")] = config.temperature;

  // Extract system message and build the messages array.
  QJsonArray msg_array;
  for (const auto &msg : messages) {
    if (msg.role == QStringLiteral("system")) {
      body[QStringLiteral("system")] = msg.content;
      continue;
    }

    if (msg.role == QStringLiteral("tool")) {
      // Tool results are sent as user messages with content blocks.
      QJsonObject tool_result;
      tool_result[QStringLiteral("type")] = QStringLiteral("tool_result");
      tool_result[QStringLiteral("tool_use_id")] = msg.tool_call_id;
      tool_result[QStringLiteral("content")] = msg.content;

      QJsonArray content_array;
      content_array.append(tool_result);

      QJsonObject user_msg;
      user_msg[QStringLiteral("role")] = QStringLiteral("user");
      user_msg[QStringLiteral("content")] = content_array;
      msg_array.append(user_msg);
      continue;
    }

    if (msg.role == QStringLiteral("assistant") &&
        !msg.tool_calls_json.isEmpty()) {
      // Assistant message that includes tool calls.
      QJsonArray content_array;
      if (!msg.content.isEmpty()) {
        QJsonObject text_block;
        text_block[QStringLiteral("type")] = QStringLiteral("text");
        text_block[QStringLiteral("text")] = msg.content;
        content_array.append(text_block);
      }

      auto calls_doc = QJsonDocument::fromJson(msg.tool_calls_json.toUtf8());
      if (calls_doc.isArray()) {
        for (const auto &val : calls_doc.array()) {
          content_array.append(val);
        }
      }

      QJsonObject assistant_msg;
      assistant_msg[QStringLiteral("role")] = QStringLiteral("assistant");
      assistant_msg[QStringLiteral("content")] = content_array;
      msg_array.append(assistant_msg);
      continue;
    }

    // Normal text message.
    QJsonObject m;
    m[QStringLiteral("role")] = msg.role;
    m[QStringLiteral("content")] = msg.content;
    msg_array.append(m);
  }
  body[QStringLiteral("messages")] = msg_array;

  // Tool definitions.
  if (!tools.isEmpty()) {
    QJsonArray tools_array;
    for (const auto &tool : tools) {
      QJsonObject t;
      t[QStringLiteral("name")] = tool.name;
      t[QStringLiteral("description")] = tool.description;
      t[QStringLiteral("input_schema")] = tool.parameters_schema;
      tools_array.append(t);
    }
    body[QStringLiteral("tools")] = tools_array;
  }

  return body;
}

LLMResponse ClaudeBackend::parseResponse(const QJsonObject &json) const {
  LLMResponse response;

  response.stop_reason = json[QStringLiteral("stop_reason")].toString();

  auto usage = json[QStringLiteral("usage")].toObject();
  response.prompt_tokens =
      usage[QStringLiteral("input_tokens")].toInt();
  response.completion_tokens =
      usage[QStringLiteral("output_tokens")].toInt();

  auto content = json[QStringLiteral("content")].toArray();
  for (const auto &block : content) {
    auto obj = block.toObject();
    auto type = obj[QStringLiteral("type")].toString();

    if (type == QStringLiteral("text")) {
      response.content += obj[QStringLiteral("text")].toString();
    } else if (type == QStringLiteral("tool_use")) {
      ToolCall call;
      call.id = obj[QStringLiteral("id")].toString();
      call.name = obj[QStringLiteral("name")].toString();
      call.arguments = obj[QStringLiteral("input")].toObject();
      response.tool_calls.append(call);
    }
  }

  return response;
}

LLMResponse ClaudeBackend::sendMessage(
    const QVector<LLMMessage> &messages,
    const QVector<ToolDefinition> &tools,
    const LLMConfig &config) {

  auto api_key = m_config.value(QStringLiteral("api_key"));
  if (api_key.isEmpty()) {
    LLMResponse err;
    err.error = QStringLiteral("No API key configured for Claude backend");
    emit requestFailed(err.error);
    return err;
  }

  auto body = buildRequestBody(messages, tools, config);
  auto json_data = QJsonDocument(body).toJson(QJsonDocument::Compact);

  QNetworkRequest request{QUrl(kApiUrl)};
  request.setHeader(QNetworkRequest::ContentTypeHeader,
                    QStringLiteral("application/json"));
  request.setRawHeader("x-api-key", api_key.toUtf8());
  request.setRawHeader("anthropic-version", "2023-06-01");

  QEventLoop loop;
  auto *reply = m_network->post(request, json_data);
  QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  loop.exec();

  LLMResponse response;
  if (reply->error() != QNetworkReply::NoError) {
    response.error = reply->errorString();
    auto response_data = reply->readAll();
    if (!response_data.isEmpty()) {
      auto doc = QJsonDocument::fromJson(response_data);
      if (doc.isObject()) {
        auto err_obj = doc.object()[QStringLiteral("error")].toObject();
        auto msg = err_obj[QStringLiteral("message")].toString();
        if (!msg.isEmpty()) {
          response.error = msg;
        }
      }
    }
    emit requestFailed(response.error);
  } else {
    auto doc = QJsonDocument::fromJson(reply->readAll());
    response = parseResponse(doc.object());
  }

  reply->deleteLater();
  return response;
}

}  // namespace mx::gui
