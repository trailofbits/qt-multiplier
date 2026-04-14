// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Interfaces/ILLMBackend.h>

#include <QMap>

class QNetworkAccessManager;
class QNetworkRequest;

namespace mx::gui {

class BedrockBackend Q_DECL_FINAL : public ILLMBackend {
  Q_OBJECT

 public:
  explicit BedrockBackend(QObject *parent = nullptr);
  ~BedrockBackend(void) override;

  QString name(void) const override;

  LLMResponse sendMessage(const QVector<LLMMessage> &messages,
                          const QVector<ToolDefinition> &tools,
                          const LLMConfig &config) override;

  bool validateCredentials(void) override;

  void setConfig(const QString &key, const QString &value);
  QString config(const QString &key) const;

 private:
  QJsonObject buildRequestBody(const QVector<LLMMessage> &messages,
                               const QVector<ToolDefinition> &tools,
                               const LLMConfig &config) const;
  LLMResponse parseResponse(const QJsonObject &json) const;

  // AWS SigV4 signing helpers.
  QByteArray sign(const QByteArray &key, const QByteArray &data) const;
  QByteArray getSignatureKey(const QString &secret_key,
                             const QString &date_stamp,
                             const QString &region,
                             const QString &service) const;
  void signRequest(QNetworkRequest &request, const QByteArray &payload,
                   const QString &region) const;

  QNetworkAccessManager *m_network;
  QMap<QString, QString> m_config;
};

}  // namespace mx::gui
