// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Explorers/AgentExplorer.h>

#include <QAction>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QToolBar>
#include <QVBoxLayout>

#include <multiplier/GUI/Interfaces/IWindowManager.h>
#include <multiplier/GUI/Interfaces/IWindowWidget.h>
#include <multiplier/GUI/Managers/ActionManager.h>
#include <multiplier/GUI/Interfaces/ILLMBackend.h>
#include <multiplier/GUI/Managers/AgentManager.h>
#include <multiplier/GUI/Managers/AgentMessage.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/LLMManager.h>
#include <multiplier/GUI/Managers/MediaManager.h>

namespace mx::gui {

struct AgentExplorer::PrivateData {
  ConfigManager &config_manager;
  IWindowManager * const manager;

  LLMManager *llm_manager{nullptr};
  AgentManager *agent_manager{nullptr};

  IWindowWidget *dock{nullptr};
  QTextEdit *conversation_display{nullptr};
  QLineEdit *input_line{nullptr};
  QPushButton *send_button{nullptr};
  QLabel *token_label{nullptr};
  QAction *pause_resume_action{nullptr};
  QAction *stop_action{nullptr};

  int64_t current_session_id{-1};
  int total_prompt_tokens{0};
  int total_completion_tokens{0};
  bool paused{false};

  inline PrivateData(ConfigManager &config_manager_, IWindowManager *manager_)
      : config_manager(config_manager_),
        manager(manager_) {}
};

AgentExplorer::~AgentExplorer(void) {}

AgentExplorer::AgentExplorer(ConfigManager &config_manager,
                             IWindowManager *parent)
    : IMainWindowPlugin(config_manager, parent),
      d(new PrivateData(config_manager, parent)) {

  // Create the LLM and Agent managers.
  d->llm_manager = new LLMManager(this);
  d->llm_manager->loadConfig();
  d->agent_manager = new AgentManager(*d->llm_manager, this);

  // Connect AgentManager signals.
  connect(d->agent_manager, &AgentManager::messageAdded,
          this, &AgentExplorer::OnMessageAdded);
  connect(d->agent_manager, &AgentManager::tokenUsageUpdated,
          this, &AgentExplorer::OnTokenUsageUpdated);
  connect(d->agent_manager, &AgentManager::sessionCompleted,
          this, &AgentExplorer::OnSessionCompleted);
  connect(d->agent_manager, &AgentManager::sessionError,
          this, &AgentExplorer::OnSessionError);

  CreateDockWidget(parent);
}

void AgentExplorer::CreateDockWidget(IWindowManager *manager) {
  d->dock = new IWindowWidget;
  d->dock->setWindowTitle(tr("Agent"));
  d->dock->setContentsMargins(0, 0, 0, 0);

  auto *layout = new QVBoxLayout(d->dock);
  layout->setContentsMargins(4, 4, 4, 4);

  // Toolbar row.
  auto *toolbar_layout = new QHBoxLayout;
  toolbar_layout->setContentsMargins(0, 0, 0, 0);

  auto *new_session_btn = new QPushButton(tr("New Session"), d->dock);
  connect(new_session_btn, &QPushButton::clicked,
          this, &AgentExplorer::OnNewSession);
  toolbar_layout->addWidget(new_session_btn);

  auto *pause_btn = new QPushButton(tr("Pause"), d->dock);
  connect(pause_btn, &QPushButton::clicked,
          this, &AgentExplorer::OnPauseResume);
  toolbar_layout->addWidget(pause_btn);
  // Store for toggling text.
  d->pause_resume_action = new QAction(this);
  d->pause_resume_action->setData(QVariant::fromValue(pause_btn));

  auto *stop_btn = new QPushButton(tr("Stop"), d->dock);
  connect(stop_btn, &QPushButton::clicked,
          this, &AgentExplorer::OnStop);
  toolbar_layout->addWidget(stop_btn);

  toolbar_layout->addStretch();

  d->token_label = new QLabel(tr("Tokens: 0 / 0"), d->dock);
  toolbar_layout->addWidget(d->token_label);

  layout->addLayout(toolbar_layout);

  // Conversation display.
  d->conversation_display = new QTextEdit(d->dock);
  d->conversation_display->setReadOnly(true);
  d->conversation_display->setAcceptRichText(true);
  layout->addWidget(d->conversation_display, 1);

  // Input row.
  auto *input_layout = new QHBoxLayout;
  input_layout->setContentsMargins(0, 0, 0, 0);

  d->input_line = new QLineEdit(d->dock);
  d->input_line->setPlaceholderText(tr("Type a message..."));
  connect(d->input_line, &QLineEdit::returnPressed,
          this, &AgentExplorer::OnSendMessage);
  input_layout->addWidget(d->input_line, 1);

  d->send_button = new QPushButton(tr("Send"), d->dock);
  connect(d->send_button, &QPushButton::clicked,
          this, &AgentExplorer::OnSendMessage);
  input_layout->addWidget(d->send_button);

  layout->addLayout(input_layout);

  d->dock->setLayout(layout);

  IWindowManager::DockConfig config;
  config.id = "com.trailofbits.dock.AgentExplorer";
  config.location = IWindowManager::DockLocation::Bottom;
  config.tabify = true;
  config.start_hidden = true;
  config.app_menu_location = {tr("View"), tr("Explorers")};
  manager->AddDockWidget(d->dock, config);
}

void AgentExplorer::OnNewSession(void) {
  // Cancel any existing session.
  if (d->current_session_id >= 0) {
    d->agent_manager->cancelSession(d->current_session_id);
  }

  d->conversation_display->clear();
  d->total_prompt_tokens = 0;
  d->total_completion_tokens = 0;
  d->token_label->setText(tr("Tokens: 0 / 0"));
  d->paused = false;

  // Create a new agent session with the active backend.
  auto backend_name = d->llm_manager->activeBackendName();
  d->current_session_id = d->agent_manager->createSession(
      tr("Session"), QString(), backend_name);

  if (d->current_session_id >= 0) {
    // Persist to DB.
    auto *backend = d->llm_manager->activeBackend();
    QString model_name = backend ? backend->name() : QString();
    d->config_manager.CreateAgentSession(
        tr("Session"), QString(), backend_name, model_name);

    d->conversation_display->append(
        QStringLiteral("<i>New session started.</i>"));
  } else {
    d->conversation_display->append(
        QStringLiteral("<b style=\"color:red\">Failed to create session. "
                       "Check LLM backend configuration.</b>"));
  }
}

void AgentExplorer::OnSendMessage(void) {
  auto text = d->input_line->text().trimmed();
  if (text.isEmpty()) return;

  // Auto-create session if needed.
  if (d->current_session_id < 0) {
    OnNewSession();
    if (d->current_session_id < 0) return;
  }

  d->input_line->clear();

  // Show user message.
  d->conversation_display->append(
      QStringLiteral("<p><b>You:</b> %1</p>")
          .arg(text.toHtmlEscaped()));

  // Persist user message.
  d->config_manager.SaveAgentMessage(
      d->current_session_id, QStringLiteral("user"), text);

  // Send to agent.
  d->agent_manager->sendMessage(d->current_session_id, text);
}

void AgentExplorer::OnPauseResume(void) {
  if (d->current_session_id < 0) return;

  if (d->paused) {
    d->agent_manager->resumeSession(d->current_session_id);
    d->paused = false;
    // Update button text.
    auto btn = d->pause_resume_action->data().value<QPushButton *>();
    if (btn) btn->setText(tr("Pause"));
    d->conversation_display->append(
        QStringLiteral("<i>Session resumed.</i>"));
  } else {
    d->agent_manager->pauseSession(d->current_session_id);
    d->paused = true;
    auto btn = d->pause_resume_action->data().value<QPushButton *>();
    if (btn) btn->setText(tr("Resume"));
    d->conversation_display->append(
        QStringLiteral("<i>Session paused.</i>"));
  }
}

void AgentExplorer::OnStop(void) {
  if (d->current_session_id < 0) return;
  d->agent_manager->cancelSession(d->current_session_id);
  d->config_manager.UpdateAgentSessionStatus(
      d->current_session_id, QStringLiteral("cancelled"));
  d->conversation_display->append(
      QStringLiteral("<i>Session stopped.</i>"));
  d->current_session_id = -1;
  d->paused = false;
  auto btn = d->pause_resume_action->data().value<QPushButton *>();
  if (btn) btn->setText(tr("Pause"));
}

void AgentExplorer::OnMessageAdded(int64_t session_id,
                                   const AgentMessage &msg) {
  if (session_id != d->current_session_id) return;

  if (msg.role == QStringLiteral("assistant")) {
    d->conversation_display->append(
        QStringLiteral("<p><b>Assistant:</b> %1</p>")
            .arg(msg.content.toHtmlEscaped()));

    // Persist.
    d->config_manager.SaveAgentMessage(
        session_id, msg.role, msg.content,
        msg.tool_name, msg.tool_call_id, {}, {},
        msg.token_count);

  } else if (msg.role == QStringLiteral("tool_call")) {
    d->conversation_display->append(
        QStringLiteral("<p style=\"color:gray\"><i>Tool call: %1</i></p>")
            .arg(msg.tool_name.toHtmlEscaped()));

  } else if (msg.role == QStringLiteral("tool_result")) {
    d->conversation_display->append(
        QStringLiteral("<p style=\"color:gray\"><i>Tool result: %1</i></p>")
            .arg(msg.content.left(200).toHtmlEscaped()));
  }
}

void AgentExplorer::OnTokenUsageUpdated(int64_t session_id,
                                        int prompt_tokens,
                                        int completion_tokens) {
  if (session_id != d->current_session_id) return;
  d->total_prompt_tokens += prompt_tokens;
  d->total_completion_tokens += completion_tokens;
  d->token_label->setText(
      tr("Tokens: %1 / %2")
          .arg(d->total_prompt_tokens)
          .arg(d->total_completion_tokens));

  d->config_manager.UpdateAgentSessionTokens(
      session_id, prompt_tokens, completion_tokens);
}

void AgentExplorer::OnSessionCompleted(int64_t session_id,
                                       const QString &summary) {
  if (session_id != d->current_session_id) return;
  d->conversation_display->append(
      QStringLiteral("<p><b style=\"color:green\">Session completed.</b> %1</p>")
          .arg(summary.toHtmlEscaped()));
  d->config_manager.UpdateAgentSessionStatus(
      session_id, QStringLiteral("completed"));
}

void AgentExplorer::OnSessionError(int64_t session_id,
                                   const QString &error) {
  if (session_id != d->current_session_id) return;
  d->conversation_display->append(
      QStringLiteral("<p><b style=\"color:red\">Error:</b> %1</p>")
          .arg(error.toHtmlEscaped()));
}

}  // namespace mx::gui
