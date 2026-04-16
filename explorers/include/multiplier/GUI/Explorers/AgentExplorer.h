// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Interfaces/IMainWindowPlugin.h>

#include <memory>

namespace mx::gui {

struct SessionResult;
class CodeExplorer;
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

  // Set the CodeExplorer used for code context tracking.
  void setCodeExplorer(CodeExplorer *explorer);

 private:
  void CreateDockWidgets(IWindowManager *manager);
  void ConnectSignals(void);
  void RegisterTools(void);
  void LoadSession(int64_t session_id);
  void StartObserver(void);
  void StopObserver(void);
  void requestRecommendation(void);
  void handleRecommendationResponse(const struct LLMResponse &response);
  void summarizeViewedCode(void);
  void handleCodeSummaryResponse(const struct LLMResponse &response);

 private slots:
  void OnSendMessage(const QString &text);
  void OnNewSession(void);
  void OnPauseResume(void);
  void OnStop(void);
  void OnToggleObserver(bool checked);
  void OnMessageAdded(int64_t session_id, const struct AgentMessage &msg);
  void OnTokenUsageUpdated(int64_t session_id, int prompt_tokens,
                           int completion_tokens);
  void OnSessionCompleted(int64_t session_id, const QString &summary);
  void OnSessionFinished(int64_t session_id, const SessionResult &result);
  void OnSessionError(int64_t session_id, const QString &error);
  void OnSessionSelected(int64_t session_id);
  void OnSessionResumeRequested(int64_t session_id);
  void OnSessionDeleteRequested(int64_t session_id);
  void OnConfigChanged(void);
  void OnToolCallStarted(int64_t session_id, const QString &name,
                         const QJsonObject &args);
  void OnToolCallCompleted(int64_t session_id, const QString &name,
                           const QJsonObject &result, int duration_ms);
  void OnObserverTriggered(int64_t observer_session_id);
};

}  // namespace mx::gui
