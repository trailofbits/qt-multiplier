// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Interfaces/ILLMBackend.h>

#include <QMap>

class QNetworkAccessManager;

namespace mx::gui {

class ClaudeBackend Q_DECL_FINAL : public ILLMBackend {
  Q_OBJECT

 public:
  explicit ClaudeBackend(QObject *parent = nullptr);
  ~ClaudeBackend(void) override;

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

  QNetworkAccessManager *m_network;
  QMap<QString, QString> m_config;
};

}  // namespace mx::gui
