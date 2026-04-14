// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Interfaces/IMainWindowPlugin.h>

#include <memory>

namespace mx::gui {

class ConfigManager;
class IWindowManager;

class AgentExplorer Q_DECL_FINAL : public IMainWindowPlugin {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  virtual ~AgentExplorer(void);

  explicit AgentExplorer(ConfigManager &config_manager,
                         IWindowManager *parent = nullptr);

 private:
  void CreateDockWidget(IWindowManager *manager);

 private slots:
  void OnSendMessage(void);
  void OnNewSession(void);
  void OnPauseResume(void);
  void OnStop(void);
  void OnMessageAdded(int64_t session_id, const class AgentMessage &msg);
  void OnTokenUsageUpdated(int64_t session_id, int prompt_tokens,
                           int completion_tokens);
  void OnSessionCompleted(int64_t session_id, const QString &summary);
  void OnSessionError(int64_t session_id, const QString &error);
};

}  // namespace mx::gui
