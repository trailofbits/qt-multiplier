// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QWidget>

#include <memory>

class QJsonObject;

namespace mx::gui {

class AgentToolLogWidget Q_DECL_FINAL : public QWidget {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  virtual ~AgentToolLogWidget(void);

  explicit AgentToolLogWidget(QWidget *parent = nullptr);

 public slots:
  void onToolCallStarted(int64_t session_id, const QString &name,
                         const QJsonObject &args);
  void onToolCallCompleted(int64_t session_id, const QString &name,
                           const QJsonObject &result, int duration_ms);
  void clear(void);
};

}  // namespace mx::gui
