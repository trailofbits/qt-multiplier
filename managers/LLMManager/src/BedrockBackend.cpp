// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "BedrockBackend.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageAuthenticationCode>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace mx::gui {

static const QString kDefaultRegion = QStringLiteral("us-east-1");
static const QString kDefaultModel =
    QStringLiteral("anthropic.claude-sonnet-4-20250514-v1:0");
static const QString kService = QStringLiteral("bedrock");

BedrockBackend::BedrockBackend(QObject *parent)
    : ILLMBackend(parent),
      m_network(new QNetworkAccessManager(this)) {}

BedrockBackend::~BedrockBackend(void) = default;

QString BedrockBackend::name(void) const {
  return QStringLiteral("AWS Bedrock");
}

void BedrockBackend::setConfig(const QString &key, const QString &value) {
  m_config[key] = value;
}

QString BedrockBackend::config(const QString &key) const {
  return m_config.value(key);
}

bool BedrockBackend::validateCredentials(void) {
  return !m_config.value(QStringLiteral("access_key_id")).isEmpty() &&
         !m_config.value(QStringLiteral("secret_access_key")).isEmpty();
}

QByteArray BedrockBackend::sign(const QByteArray &key,
                                const QByteArray &data) const {
  return QMessageAuthenticationCode::hash(
      data, key, QCryptographicHash::Sha256);
}

QByteArray BedrockBackend::getSignatureKey(
    const QString &secret_key, const QString &date_stamp,
    const QString &region, const QString &service) const {
  auto k_date = sign(
      QByteArray("AWS4") + secret_key.toUtf8(), date_stamp.toUtf8());
  auto k_region = sign(k_date, region.toUtf8());
  auto k_service = sign(k_region, service.toUtf8());
  return sign(k_service, QByteArray("aws4_request"));
}

void BedrockBackend::signRequest(QNetworkRequest &request,
                                 const QByteArray &payload,
                                 const QString &region) const {
  auto access_key = m_config.value(QStringLiteral("access_key_id"));
  auto secret_key = m_config.value(QStringLiteral("secret_access_key"));

  auto now = QDateTime::currentDateTimeUtc();
  auto amz_date = now.toString(QStringLiteral("yyyyMMddTHHmmssZ"));
  auto date_stamp = now.toString(QStringLiteral("yyyyMMdd"));

  auto url = request.url();
  auto host = url.host();
  auto path = url.path();

  auto payload_hash = QCryptographicHash::hash(
      payload, QCryptographicHash::Sha256).toHex();

  request.setRawHeader("x-amz-date", amz_date.toUtf8());
  request.setRawHeader("x-amz-content-sha256", payload_hash);

  auto session_token =
      m_config.value(QStringLiteral("session_token"));
  if (!session_token.isEmpty()) {
    request.setRawHeader("x-amz-security-token", session_token.toUtf8());
  }

  // Canonical request.
  QString signed_headers;
  QString canonical_headers;
  if (session_token.isEmpty()) {
    signed_headers = QStringLiteral(
        "content-type;host;x-amz-content-sha256;x-amz-date");
    canonical_headers = QStringLiteral("content-type:application/json\n")
                        + QStringLiteral("host:") + host
                        + QStringLiteral("\n")
                        + QStringLiteral("x-amz-content-sha256:")
                        + QString::fromUtf8(payload_hash)
                        + QStringLiteral("\n")
                        + QStringLiteral("x-amz-date:") + amz_date
                        + QStringLiteral("\n");
  } else {
    signed_headers = QStringLiteral(
        "content-type;host;x-amz-content-sha256;x-amz-date;"
        "x-amz-security-token");
    canonical_headers = QStringLiteral("content-type:application/json\n")
                        + QStringLiteral("host:") + host
                        + QStringLiteral("\n")
                        + QStringLiteral("x-amz-content-sha256:")
                        + QString::fromUtf8(payload_hash)
                        + QStringLiteral("\n")
                        + QStringLiteral("x-amz-date:") + amz_date
                        + QStringLiteral("\n")
                        + QStringLiteral("x-amz-security-token:")
                        + session_token + QStringLiteral("\n");
  }

  auto canonical_request =
      QStringLiteral("POST\n") + path + QStringLiteral("\n\n")
      + canonical_headers + QStringLiteral("\n") + signed_headers
      + QStringLiteral("\n") + QString::fromUtf8(payload_hash);

  auto canonical_hash = QCryptographicHash::hash(
      canonical_request.toUtf8(), QCryptographicHash::Sha256).toHex();

  auto credential_scope = date_stamp + QStringLiteral("/") + region
      + QStringLiteral("/") + kService
      + QStringLiteral("/aws4_request");

  auto string_to_sign =
      QStringLiteral("AWS4-HMAC-SHA256\n") + amz_date
      + QStringLiteral("\n") + credential_scope + QStringLiteral("\n")
      + QString::fromUtf8(canonical_hash);

  auto signing_key = getSignatureKey(secret_key, date_stamp, region, kService);
  auto signature = QMessageAuthenticationCode::hash(
      string_to_sign.toUtf8(), signing_key,
      QCryptographicHash::Sha256).toHex();

  auto auth_header =
      QStringLiteral("AWS4-HMAC-SHA256 Credential=")
      + access_key + QStringLiteral("/") + credential_scope
      + QStringLiteral(", SignedHeaders=") + signed_headers
      + QStringLiteral(", Signature=") + QString::fromUtf8(signature);

  request.setRawHeader("Authorization", auth_header.toUtf8());
}

QJsonObject BedrockBackend::buildRequestBody(
    const QVector<LLMMessage> &messages,
    const QVector<ToolDefinition> &tools,
    const LLMConfig &config) const {

  QJsonObject body;

  QJsonObject inference_config;
  inference_config[QStringLiteral("maxTokens")] = config.max_tokens;
  inference_config[QStringLiteral("temperature")] = config.temperature;
  body[QStringLiteral("inferenceConfig")] = inference_config;

  // Build messages. Extract system to top-level.
  QJsonArray msg_array;
  for (const auto &msg : messages) {
    if (msg.role == QStringLiteral("system")) {
      QJsonArray system_array;
      QJsonObject text_block;
      text_block[QStringLiteral("text")] = msg.content;
      system_array.append(text_block);
      body[QStringLiteral("system")] = system_array;
      continue;
    }

    if (msg.role == QStringLiteral("tool")) {
      QJsonObject tool_result;
      tool_result[QStringLiteral("toolUseId")] = msg.tool_call_id;

      QJsonObject content_block;
      // Try to parse as JSON; if it fails, wrap as text.
      auto doc = QJsonDocument::fromJson(msg.content.toUtf8());
      if (doc.isObject()) {
        content_block[QStringLiteral("json")] = doc.object();
      } else {
        content_block[QStringLiteral("text")] = msg.content;
      }

      QJsonArray content_array;
      content_array.append(content_block);
      tool_result[QStringLiteral("content")] = content_array;
      tool_result[QStringLiteral("status")] = QStringLiteral("success");

      QJsonObject result_wrapper;
      result_wrapper[QStringLiteral("toolResult")] = tool_result;

      QJsonArray user_content;
      user_content.append(result_wrapper);

      QJsonObject user_msg;
      user_msg[QStringLiteral("role")] = QStringLiteral("user");
      user_msg[QStringLiteral("content")] = user_content;
      msg_array.append(user_msg);
      continue;
    }

    if (msg.role == QStringLiteral("assistant") &&
        !msg.tool_calls_json.isEmpty()) {
      QJsonArray content_array;
      if (!msg.content.isEmpty()) {
        QJsonObject text_block;
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

    QJsonArray content_array;
    QJsonObject text_block;
    text_block[QStringLiteral("text")] = msg.content;
    content_array.append(text_block);

    QJsonObject m;
    m[QStringLiteral("role")] = msg.role;
    m[QStringLiteral("content")] = content_array;
    msg_array.append(m);
  }
  body[QStringLiteral("messages")] = msg_array;

  // Tool definitions.
  if (!tools.isEmpty()) {
    QJsonObject tool_config;
    QJsonArray tools_array;
    for (const auto &tool : tools) {
      QJsonObject spec;
      spec[QStringLiteral("name")] = tool.name;
      spec[QStringLiteral("description")] = tool.description;
      spec[QStringLiteral("inputSchema")] = QJsonObject{
          {QStringLiteral("json"), tool.parameters_schema}};

      QJsonObject t;
      t[QStringLiteral("toolSpec")] = spec;
      tools_array.append(t);
    }
    tool_config[QStringLiteral("tools")] = tools_array;
    body[QStringLiteral("toolConfig")] = tool_config;
  }

  return body;
}

LLMResponse BedrockBackend::parseResponse(const QJsonObject &json) const {
  LLMResponse response;

  response.stop_reason = json[QStringLiteral("stopReason")].toString();

  auto usage = json[QStringLiteral("usage")].toObject();
  response.prompt_tokens =
      usage[QStringLiteral("inputTokens")].toInt();
  response.completion_tokens =
      usage[QStringLiteral("outputTokens")].toInt();

  auto output = json[QStringLiteral("output")].toObject();
  auto message = output[QStringLiteral("message")].toObject();
  auto content = message[QStringLiteral("content")].toArray();

  for (const auto &block : content) {
    auto obj = block.toObject();
    if (obj.contains(QStringLiteral("text"))) {
      response.content += obj[QStringLiteral("text")].toString();
    } else if (obj.contains(QStringLiteral("toolUse"))) {
      auto tool_use = obj[QStringLiteral("toolUse")].toObject();
      ToolCall call;
      call.id = tool_use[QStringLiteral("toolUseId")].toString();
      call.name = tool_use[QStringLiteral("name")].toString();
      call.arguments = tool_use[QStringLiteral("input")].toObject();
      response.tool_calls.append(call);
    }
  }

  return response;
}

LLMResponse BedrockBackend::sendMessage(
    const QVector<LLMMessage> &messages,
    const QVector<ToolDefinition> &tools,
    const LLMConfig &config) {

  if (!validateCredentials()) {
    LLMResponse err;
    err.error = QStringLiteral(
        "No AWS credentials configured for Bedrock backend");
    emit requestFailed(err.error);
    return err;
  }

  auto region =
      m_config.value(QStringLiteral("region"), kDefaultRegion);
  QString model = config.model;
  if (model.isEmpty()) {
    model = m_config.value(QStringLiteral("model"), kDefaultModel);
  }

  auto url = QStringLiteral("https://bedrock-runtime.%1.amazonaws.com"
                             "/model/%2/converse")
                 .arg(region, model);

  auto body = buildRequestBody(messages, tools, config);
  auto json_data = QJsonDocument(body).toJson(QJsonDocument::Compact);

  QNetworkRequest request{QUrl(url)};
  request.setHeader(QNetworkRequest::ContentTypeHeader,
                    QStringLiteral("application/json"));
  signRequest(request, json_data, region);

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
        auto msg = doc.object()[QStringLiteral("message")].toString();
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
