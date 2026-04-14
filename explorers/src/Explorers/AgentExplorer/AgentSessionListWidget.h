// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QWidget>

#include <memory>

namespace mx::gui {

class ConfigManager;

class AgentSessionListWidget Q_DECL_FINAL : public QWidget {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  virtual ~AgentSessionListWidget(void);

  explicit AgentSessionListWidget(ConfigManager &config_manager,
                                   QWidget *parent = nullptr);

  void refresh(void);

 signals:
  void sessionSelected(int64_t session_id);
  void sessionResumeRequested(int64_t session_id);
  void sessionDeleteRequested(int64_t session_id);

 private slots:
  void onItemDoubleClicked(int row);
  void onContextMenu(const QPoint &pos);
};

}  // namespace mx::gui
