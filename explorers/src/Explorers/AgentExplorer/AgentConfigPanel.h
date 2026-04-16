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
class LLMManager;

class AgentConfigPanel Q_DECL_FINAL : public QWidget {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  virtual ~AgentConfigPanel(void);

  explicit AgentConfigPanel(LLMManager &llm_manager,
                            ConfigManager &config_manager,
                            QWidget *parent = nullptr);

  QString systemPrompt(void) const;
  int maxIterations(void) const;
  double temperature(void) const;
  int suggestionMode(void) const;
  bool enterToSend(void) const;
  QString recommenderModel(void) const;
  QString summarizerModel(void) const;
  QString observerModel(void) const;

 signals:
  void configChanged(void);

 private slots:
  void onBackendTypeChanged(int index);
  void onApiKeyChanged(void);
  void onBaseUrlChanged(void);
  void onModelChanged(void);
  void onLoadPromptClicked(void);
  void onBrowsePythonClicked(void);

 private:
  void populateModels(const QString &backend_type);
  void ensureBackendExists(const QString &type);
  void showSaved(void);
  void maybeVerifyPython(void);
  void setPythonStatus(int state);
};

}  // namespace mx::gui
