// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "OpenAICompatBackend.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace mx::gui {

static const QString kDefaultBaseUrl =
    QStringLiteral("https://api.openai.com/v1");
static const QString kDefaultModel =
    QStringLiteral("gpt-4o");

OpenAICompatBackend::OpenAICompatBackend(QObject *parent)
    : ILLMBackend(parent),
      m_network(new QNetworkAccessManager(this)) {}

OpenAICompatBackend::~OpenAICompatBackend(void) = default;

QString OpenAICompatBackend::name(void) const {
  return QStringLiteral("OpenAI Compatible");
}

void OpenAICompatBackend::setConfig(const QString &key, const QString &value) {
  m_config[key] = value;
}

QString OpenAICompatBackend::config(const QString &key) const {
  return m_config.value(key);
}

bool OpenAICompatBackend::validateCredentials(void) {
  return !m_config.value(QStringLiteral("api_key")).isEmpty();
}

QJsonObject OpenAICompatBackend::buildRequestBody(
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

  QJsonArray msg_array;
  for (const auto &msg : messages) {
    if (msg.role == QStringLiteral("tool")) {
      QJsonObject m;
      m[QStringLiteral("role")] = QStringLiteral("tool");
      m[QStringLiteral("content")] = msg.content;
      m[QStringLiteral("tool_call_id")] = msg.tool_call_id;
      msg_array.append(m);
      continue;
    }

    if (msg.role == QStringLiteral("assistant") &&
        !msg.tool_calls_json.isEmpty()) {
      QJsonObject m;
      m[QStringLiteral("role")] = QStringLiteral("assistant");
      if (!msg.content.isEmpty()) {
        m[QStringLiteral("content")] = msg.content;
      } else {
        m[QStringLiteral("content")] = QJsonValue(QJsonValue::Null);
      }

      auto calls_doc = QJsonDocument::fromJson(msg.tool_calls_json.toUtf8());
      if (calls_doc.isArray()) {
        m[QStringLiteral("tool_calls")] = calls_doc.array();
      }
      msg_array.append(m);
      continue;
    }

    QJsonObject m;
    m[QStringLiteral("role")] = msg.role;
    m[QStringLiteral("content")] = msg.content;
    msg_array.append(m);
  }
  body[QStringLiteral("messages")] = msg_array;

  if (!tools.isEmpty()) {
    QJsonArray tools_array;
    for (const auto &tool : tools) {
      QJsonObject func;
      func[QStringLiteral("name")] = tool.name;
      func[QStringLiteral("description")] = tool.description;
      func[QStringLiteral("parameters")] = tool.parameters_schema;

      QJsonObject t;
      t[QStringLiteral("type")] = QStringLiteral("function");
      t[QStringLiteral("function")] = func;
      tools_array.append(t);
    }
    body[QStringLiteral("tools")] = tools_array;
  }

  return body;
}

LLMResponse OpenAICompatBackend::parseResponse(
    const QJsonObject &json) const {

  LLMResponse response;

  auto usage = json[QStringLiteral("usage")].toObject();
  response.prompt_tokens =
      usage[QStringLiteral("prompt_tokens")].toInt();
  response.completion_tokens =
      usage[QStringLiteral("completion_tokens")].toInt();

  auto choices = json[QStringLiteral("choices")].toArray();
  if (choices.isEmpty()) {
    response.error = QStringLiteral("No choices in response");
    return response;
  }

  auto choice = choices[0].toObject();
  response.stop_reason =
      choice[QStringLiteral("finish_reason")].toString();

  auto message = choice[QStringLiteral("message")].toObject();
  response.content = message[QStringLiteral("content")].toString();

  auto tool_calls = message[QStringLiteral("tool_calls")].toArray();
  for (const auto &tc : tool_calls) {
    auto tc_obj = tc.toObject();
    ToolCall call;
    call.id = tc_obj[QStringLiteral("id")].toString();

    auto func = tc_obj[QStringLiteral("function")].toObject();
    call.name = func[QStringLiteral("name")].toString();

    auto args_str = func[QStringLiteral("arguments")].toString();
    auto args_doc = QJsonDocument::fromJson(args_str.toUtf8());
    if (args_doc.isObject()) {
      call.arguments = args_doc.object();
    }

    response.tool_calls.append(call);
  }

  return response;
}

LLMResponse OpenAICompatBackend::sendMessage(
    const QVector<LLMMessage> &messages,
    const QVector<ToolDefinition> &tools,
    const LLMConfig &config) {

  auto api_key = m_config.value(QStringLiteral("api_key"));
  if (api_key.isEmpty()) {
    LLMResponse err;
    err.error = QStringLiteral("No API key configured for OpenAI backend");
    emit requestFailed(err.error);
    return err;
  }

  auto base_url =
      m_config.value(QStringLiteral("base_url"), kDefaultBaseUrl);
  auto url = base_url + QStringLiteral("/chat/completions");

  auto body = buildRequestBody(messages, tools, config);
  auto json_data = QJsonDocument(body).toJson(QJsonDocument::Compact);

  QNetworkRequest request{QUrl(url)};
  request.setHeader(QNetworkRequest::ContentTypeHeader,
                    QStringLiteral("application/json"));
  request.setRawHeader("Authorization",
                       QStringLiteral("Bearer %1").arg(api_key).toUtf8());

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
