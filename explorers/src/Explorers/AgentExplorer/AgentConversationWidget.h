// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QJsonObject>
#include <QStringList>
#include <QWidget>

#include <memory>

namespace mx::gui {

struct AgentMessage;
class ThemeManager;

class AgentConversationWidget Q_DECL_FINAL : public QWidget {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  virtual ~AgentConversationWidget(void);

  explicit AgentConversationWidget(ThemeManager &theme_manager,
                                   QWidget *parent = nullptr);

  void setEnterToSend(bool enabled);

 signals:
  void sendMessageRequested(const QString &text);
  void suggestionAccepted(const QString &text);

 public slots:
  void addMessage(const AgentMessage &msg);
  void updateTokens(int prompt_tokens, int completion_tokens,
                    double cost_usd = -1.0);
  void clear(void);
  void showSuggestion(const QString &suggestion,
                      const QStringList &alternatives = {});
  void clearSuggestion(void);

 private slots:
  void onSendClicked(void);
  void onThemeChanged(const ThemeManager &tm);

 protected:
  bool eventFilter(QObject *obj, QEvent *event) override;

 private:
  void addMessageBubble(const QString &role, const QString &content,
                        const QString &tool_name = {},
                        const QJsonObject &tool_args = {},
                        const QJsonObject &tool_result = {});
  void scrollToBottom(void);
  void applyThemeColors(void);
};

}  // namespace mx::gui
