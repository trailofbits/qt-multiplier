// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Explorers/AgentExplorer.h>

#include <QAction>
#include <QDateTime>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QThread>
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

#include "AgentConversationWidget.h"
#include "AgentConfigPanel.h"
#include "AgentToolLogWidget.h"
#include "AgentSessionListWidget.h"

namespace mx::gui {

struct AgentExplorer::PrivateData {
  ConfigManager &config_manager;
  IWindowManager * const manager;

  LLMManager *llm_manager{nullptr};
  AgentManager *agent_manager{nullptr};

  // Dock widgets.
  IWindowWidget *main_dock{nullptr};
  IWindowWidget *config_dock{nullptr};
  IWindowWidget *tool_log_dock{nullptr};
  IWindowWidget *session_list_dock{nullptr};

  // Child widgets.
  AgentConversationWidget *conversation{nullptr};
  AgentConfigPanel *config_panel{nullptr};
  AgentToolLogWidget *tool_log{nullptr};
  AgentSessionListWidget *session_list{nullptr};

  // Toolbar buttons.
  QPushButton *new_session_btn{nullptr};
  QPushButton *pause_btn{nullptr};
  QPushButton *stop_btn{nullptr};
  QPushButton *observer_btn{nullptr};
  QLabel *observer_label{nullptr};

  struct RecommenderState {
    QString context_summary;
    QStringList open_questions;
    QString current_suggestion;
    QStringList alternatives;
    int suggestion_tokens{0};
  };
  RecommenderState recommender;

  int64_t current_session_id{-1};
  bool paused{false};

  // Observer mode.
  int64_t observer_session_id{-1};
  int observer_tool_call_count{0};
  int observer_trigger_interval{5};  // trigger every N primary tool calls
  int observer_recommendation_count{0};
  bool observer_enabled{false};

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

  // Register all built-in tools.
  RegisterTools();

  // Build UI.
  CreateDockWidgets(parent);

  // Wire signals.
  ConnectSignals();
}

void AgentExplorer::RegisterTools(void) {
  d->agent_manager->registerBuiltinTools(d->config_manager);
}

void AgentExplorer::CreateDockWidgets(IWindowManager *manager) {
  auto &theme_manager = d->config_manager.ThemeManager();

  // Main dock: conversation.
  d->main_dock = new IWindowWidget;
  d->main_dock->setWindowTitle(tr("Agent"));
  d->main_dock->setContentsMargins(0, 0, 0, 0);

  auto *main_layout = new QVBoxLayout(d->main_dock);
  main_layout->setContentsMargins(4, 4, 4, 4);
  main_layout->setSpacing(4);

  // Toolbar.
  auto *toolbar_layout = new QHBoxLayout;
  toolbar_layout->setContentsMargins(0, 0, 0, 0);

  d->new_session_btn = new QPushButton(tr("New Session"), d->main_dock);
  d->new_session_btn->setToolTip(
      tr("Start a fresh conversation with the configured LLM backend"));
  toolbar_layout->addWidget(d->new_session_btn);

  d->pause_btn = new QPushButton(tr("Pause"), d->main_dock);
  d->pause_btn->setToolTip(
      tr("Pause the agent's tool-calling loop (click again to resume)"));
  toolbar_layout->addWidget(d->pause_btn);

  d->stop_btn = new QPushButton(tr("Stop"), d->main_dock);
  d->stop_btn->setToolTip(
      tr("Cancel the current agent session"));
  toolbar_layout->addWidget(d->stop_btn);

  d->observer_btn = new QPushButton(tr("Observer"), d->main_dock);
  d->observer_btn->setCheckable(true);
  d->observer_btn->setToolTip(
      tr("Enable a secondary agent that periodically reviews the primary "
         "agent's work, identifies gaps, and writes recommendations "
         "to an observer notes document"));
  toolbar_layout->addWidget(d->observer_btn);

  d->observer_label = new QLabel(d->main_dock);
  d->observer_label->setVisible(false);
  auto obs_font = d->observer_label->font();
  obs_font.setPointSize(obs_font.pointSize() - 1);
  d->observer_label->setFont(obs_font);
  toolbar_layout->addWidget(d->observer_label);

  toolbar_layout->addStretch();
  main_layout->addLayout(toolbar_layout);

  // Conversation widget.
  d->conversation = new AgentConversationWidget(theme_manager, d->main_dock);
  main_layout->addWidget(d->conversation, 1);

  {
    IWindowManager::DockConfig config;
    config.id = "com.trailofbits.dock.AgentExplorer";
    config.location = IWindowManager::DockLocation::Bottom;
    config.tabify = true;
    config.start_hidden = true;
    config.app_menu_location = {tr("Agent")};
    manager->AddDockWidget(d->main_dock, config);
  }

  // Right dock: config panel.
  d->config_dock = new IWindowWidget;
  d->config_dock->setWindowTitle(tr("Agent Config"));
  d->config_dock->setContentsMargins(0, 0, 0, 0);

  auto *config_layout = new QVBoxLayout(d->config_dock);
  config_layout->setContentsMargins(0, 0, 0, 0);

  d->config_panel = new AgentConfigPanel(
      *d->llm_manager, d->config_manager, d->config_dock);
  config_layout->addWidget(d->config_panel);

  {
    IWindowManager::DockConfig config;
    config.id = "com.trailofbits.dock.AgentConfig";
    config.location = IWindowManager::DockLocation::Right;
    config.tabify = true;
    config.start_hidden = true;
    config.app_menu_location = {tr("Agent")};
    manager->AddDockWidget(d->config_dock, config);
  }

  // Bottom dock: tool log.
  d->tool_log_dock = new IWindowWidget;
  d->tool_log_dock->setWindowTitle(tr("Agent Tool Log"));
  d->tool_log_dock->setContentsMargins(0, 0, 0, 0);

  auto *tool_layout = new QVBoxLayout(d->tool_log_dock);
  tool_layout->setContentsMargins(0, 0, 0, 0);

  d->tool_log = new AgentToolLogWidget(d->tool_log_dock);
  tool_layout->addWidget(d->tool_log);

  {
    IWindowManager::DockConfig config;
    config.id = "com.trailofbits.dock.AgentToolLog";
    config.location = IWindowManager::DockLocation::Bottom;
    config.tabify = true;
    config.start_hidden = true;
    config.app_menu_location = {tr("Agent")};
    manager->AddDockWidget(d->tool_log_dock, config);
  }

  // Left dock: session list.
  d->session_list_dock = new IWindowWidget;
  d->session_list_dock->setWindowTitle(tr("Agent Sessions"));
  d->session_list_dock->setContentsMargins(0, 0, 0, 0);

  auto *session_layout = new QVBoxLayout(d->session_list_dock);
  session_layout->setContentsMargins(0, 0, 0, 0);

  d->session_list = new AgentSessionListWidget(
      d->config_manager, d->session_list_dock);
  session_layout->addWidget(d->session_list);

  {
    IWindowManager::DockConfig config;
    config.id = "com.trailofbits.dock.AgentSessions";
    config.location = IWindowManager::DockLocation::Left;
    config.tabify = true;
    config.start_hidden = true;
    config.app_menu_location = {tr("Agent")};
    manager->AddDockWidget(d->session_list_dock, config);
  }
}

void AgentExplorer::ConnectSignals(void) {
  // Toolbar.
  connect(d->new_session_btn, &QPushButton::clicked,
          this, &AgentExplorer::OnNewSession);
  connect(d->pause_btn, &QPushButton::clicked,
          this, &AgentExplorer::OnPauseResume);
  connect(d->stop_btn, &QPushButton::clicked,
          this, &AgentExplorer::OnStop);
  connect(d->observer_btn, &QPushButton::toggled,
          this, &AgentExplorer::OnToggleObserver);

  // Conversation send.
  connect(d->conversation, &AgentConversationWidget::sendMessageRequested,
          this, &AgentExplorer::OnSendMessage);

  // AgentManager signals.
  connect(d->agent_manager, &AgentManager::messageAdded,
          this, &AgentExplorer::OnMessageAdded);
  connect(d->agent_manager, &AgentManager::tokenUsageUpdated,
          this, &AgentExplorer::OnTokenUsageUpdated);
  connect(d->agent_manager, &AgentManager::sessionCompleted,
          this, &AgentExplorer::OnSessionCompleted);
  connect(d->agent_manager, &AgentManager::sessionFinished,
          this, &AgentExplorer::OnSessionFinished);
  connect(d->agent_manager, &AgentManager::sessionError,
          this, &AgentExplorer::OnSessionError);
  connect(d->agent_manager, &AgentManager::toolCallStarted,
          this, &AgentExplorer::OnToolCallStarted);
  connect(d->agent_manager, &AgentManager::toolCallCompleted,
          this, &AgentExplorer::OnToolCallCompleted);
  connect(d->agent_manager, &AgentManager::observerTriggered,
          this, &AgentExplorer::OnObserverTriggered);

  // Session list.
  connect(d->session_list, &AgentSessionListWidget::sessionSelected,
          this, &AgentExplorer::OnSessionSelected);
  connect(d->session_list, &AgentSessionListWidget::sessionResumeRequested,
          this, &AgentExplorer::OnSessionResumeRequested);
  connect(d->session_list, &AgentSessionListWidget::sessionDeleteRequested,
          this, &AgentExplorer::OnSessionDeleteRequested);

  // Config changes.
  connect(d->config_panel, &AgentConfigPanel::configChanged,
          this, &AgentExplorer::OnConfigChanged);

  // Recommender: fire after each session completion.
  connect(d->agent_manager, &AgentManager::sessionCompleted,
          this, [this](int64_t sid, const QString &) {
            if (sid == d->current_session_id) {
              requestRecommendation();
            }
          });
}

void AgentExplorer::OnNewSession(void) {
  // Cancel any existing session and observer.
  StopObserver();
  if (d->current_session_id >= 0) {
    d->agent_manager->cancelSession(d->current_session_id);
  }

  d->conversation->clear();
  d->tool_log->clear();
  d->paused = false;
  d->pause_btn->setText(tr("Pause"));
  d->observer_tool_call_count = 0;

  // Apply current config.
  OnConfigChanged();

  auto system_prompt = d->config_panel->systemPrompt();
  auto backend_name = d->llm_manager->activeBackendName();
  d->current_session_id = d->agent_manager->createSession(
      tr("Session"), system_prompt, backend_name);

  if (d->current_session_id >= 0) {
    auto *backend = d->llm_manager->activeBackend();
    QString model_name = backend ? backend->name() : QString();
    d->config_manager.CreateAgentSession(
        tr("Session"), system_prompt, backend_name, model_name);
    d->session_list->refresh();
  }
}

void AgentExplorer::OnSendMessage(const QString &text) {
  if (text.isEmpty()) {
    return;
  }

  // Auto-create session if needed.
  if (d->current_session_id < 0) {
    OnNewSession();
    if (d->current_session_id < 0) {
      // Show error in conversation when no backend is configured.
      AgentMessage err_msg;
      err_msg.role = QStringLiteral("system");
      err_msg.content = tr(
          "No LLM backend is configured. Please open the Agent Config panel "
          "and add a backend with valid credentials.");
      d->conversation->addMessage(err_msg);
      return;
    }
  }

  // Persist user message.
  d->config_manager.SaveAgentMessage(
      d->current_session_id, QStringLiteral("user"), text);

  // Send to agent. The AgentManager will emit messageAdded, which
  // triggers OnMessageAdded to display it in the conversation.
  d->agent_manager->sendMessage(d->current_session_id, text);
}

void AgentExplorer::OnPauseResume(void) {
  if (d->current_session_id < 0) {
    return;
  }

  if (d->paused) {
    d->agent_manager->resumeSession(d->current_session_id);
    d->paused = false;
    d->pause_btn->setText(tr("Pause"));
  } else {
    d->agent_manager->pauseSession(d->current_session_id);
    d->paused = true;
    d->pause_btn->setText(tr("Resume"));
  }
}

void AgentExplorer::OnStop(void) {
  if (d->current_session_id < 0) {
    return;
  }
  StopObserver();
  d->agent_manager->cancelSession(d->current_session_id);
  d->config_manager.UpdateAgentSessionStatus(
      d->current_session_id, QStringLiteral("cancelled"));
  d->current_session_id = -1;
  d->paused = false;
  d->pause_btn->setText(tr("Pause"));
  d->observer_tool_call_count = 0;
  d->session_list->refresh();
}

void AgentExplorer::OnMessageAdded(int64_t session_id,
                                    const AgentMessage &msg) {
  if (session_id != d->current_session_id) {
    return;
  }

  d->conversation->addMessage(msg);

  // Persist assistant messages.
  if (msg.role == QStringLiteral("assistant")) {
    d->config_manager.SaveAgentMessage(
        session_id, msg.role, msg.content,
        msg.tool_name, msg.tool_call_id, {}, {},
        msg.token_count);
  }
}

void AgentExplorer::OnTokenUsageUpdated(int64_t session_id,
                                         int prompt_tokens,
                                         int completion_tokens) {
  if (session_id != d->current_session_id) {
    return;
  }
  d->conversation->updateTokens(prompt_tokens, completion_tokens);
  d->config_manager.UpdateAgentSessionTokens(
      session_id, prompt_tokens, completion_tokens);
}

void AgentExplorer::OnSessionCompleted(int64_t session_id,
                                        const QString &summary) {
  if (session_id != d->current_session_id) {
    return;
  }

  AgentMessage msg;
  msg.role = QStringLiteral("system");
  msg.content = tr("Session completed. %1").arg(summary);
  d->conversation->addMessage(msg);

  d->config_manager.UpdateAgentSessionStatus(
      session_id, QStringLiteral("completed"));
  d->session_list->refresh();
}

void AgentExplorer::OnSessionFinished(int64_t session_id,
                                       const SessionResult &result) {
  if (session_id != d->current_session_id) {
    return;
  }

  // Show summary as a system message.
  AgentMessage summary_msg;
  summary_msg.role = QStringLiteral("system");

  if (result.status == QStringLiteral("blocked")) {
    summary_msg.content =
        tr("Blocked: %1").arg(result.summary);
  } else if (result.status == QStringLiteral("needs_input")) {
    summary_msg.content =
        tr("Needs input: %1").arg(result.summary);
  } else {
    summary_msg.content =
        tr("Completed: %1").arg(result.summary);
  }
  d->conversation->addMessage(summary_msg);

  // Show next actions in the suggestion bar.
  if (!result.next_actions.isEmpty()) {
    d->conversation->showSuggestion(
        result.next_actions.first(),
        result.next_actions.mid(1));
  }

  d->config_manager.UpdateAgentSessionStatus(
      session_id, result.status.isEmpty() ? QStringLiteral("completed")
                                          : result.status);
  d->session_list->refresh();
}

void AgentExplorer::OnSessionError(int64_t session_id,
                                    const QString &error) {
  if (session_id != d->current_session_id) {
    return;
  }

  AgentMessage msg;
  msg.role = QStringLiteral("system");
  msg.content = tr("Error: %1").arg(error);
  d->conversation->addMessage(msg);
}

void AgentExplorer::OnSessionSelected(int64_t session_id) {
  LoadSession(session_id);
}

void AgentExplorer::OnSessionResumeRequested(int64_t session_id) {
  d->agent_manager->resumeSession(session_id);
  d->config_manager.UpdateAgentSessionStatus(
      session_id, QStringLiteral("active"));
  d->session_list->refresh();
}

void AgentExplorer::OnSessionDeleteRequested(int64_t session_id) {
  if (session_id == d->current_session_id) {
    d->agent_manager->cancelSession(session_id);
    d->current_session_id = -1;
    d->conversation->clear();
    d->tool_log->clear();
  }
  d->config_manager.DeleteAgentSession(session_id);
  d->session_list->refresh();
}

void AgentExplorer::OnConfigChanged(void) {
  LLMConfig config;
  config.temperature = d->config_panel->temperature();
  config.model = d->llm_manager->backendConfig(
      d->llm_manager->activeBackendName(), QStringLiteral("model"));
  d->agent_manager->setLLMConfig(config);
  d->agent_manager->setMaxIterations(d->config_panel->maxIterations());
}

void AgentExplorer::OnToolCallStarted(int64_t session_id, const QString &name,
                                       const QJsonObject &args) {
  if (session_id != d->current_session_id) {
    return;
  }
  d->tool_log->onToolCallStarted(session_id, name, args);

  // Also add to conversation as a collapsible tool_call message.
  AgentMessage msg;
  msg.session_id = session_id;
  msg.role = QStringLiteral("tool_call");
  msg.tool_name = name;
  msg.tool_args = args;
  d->conversation->addMessage(msg);
}

void AgentExplorer::OnToolCallCompleted(int64_t session_id,
                                         const QString &name,
                                         const QJsonObject &result,
                                         int duration_ms) {
  if (session_id == d->current_session_id) {
    d->tool_log->onToolCallCompleted(session_id, name, result, duration_ms);

    // Track tool calls for observer triggering.
    if (d->observer_enabled && d->observer_session_id >= 0) {
      d->observer_tool_call_count++;
      if (d->observer_tool_call_count >= d->observer_trigger_interval) {
        d->observer_tool_call_count = 0;
        d->agent_manager->triggerObserver(d->observer_session_id);
      }
    }
  } else if (session_id == d->observer_session_id) {
    // Observer completed a tool call -- check if it was a recommendation.
    if (name == QStringLiteral("observer_recommendation") &&
        !result.contains(QStringLiteral("error"))) {
      d->observer_recommendation_count++;
      d->observer_label->setText(
          tr("Observer: %1 recommendations")
              .arg(d->observer_recommendation_count));
    }
  }
}

void AgentExplorer::LoadSession(int64_t session_id) {
  d->current_session_id = session_id;
  d->conversation->clear();
  d->tool_log->clear();

  // Load persisted messages.
  auto messages = d->config_manager.LoadAgentMessages(session_id);
  for (const auto &info : messages) {
    AgentMessage msg;
    msg.message_id = info.message_id;
    msg.session_id = info.session_id;
    msg.role = info.role;
    msg.content = info.content;
    msg.tool_name = info.tool_name;
    msg.tool_call_id = info.tool_call_id;
    msg.token_count = info.token_count;
    d->conversation->addMessage(msg);
  }
}

void AgentExplorer::OnToggleObserver(bool checked) {
  if (checked) {
    StartObserver();
  } else {
    StopObserver();
  }
}

void AgentExplorer::StartObserver(void) {
  if (d->current_session_id < 0) {
    d->observer_btn->setChecked(false);
    return;
  }

  if (d->observer_session_id >= 0) {
    return;  // Already running.
  }

  static const QString kObserverSystemPrompt = QStringLiteral(
      "You are an observer agent reviewing another AI agent's work. "
      "Your job is to:\n"
      "1. Analyze the primary agent's approach and progress\n"
      "2. Identify gaps, missed opportunities, or potential issues\n"
      "3. Suggest improvements or alternative approaches\n"
      "4. Track whether the primary agent is staying focused on its goals\n\n"
      "Use get_primary_session_context to see what the primary agent has done.\n"
      "Use observer_recommendation to record your findings.\n\n"
      "Be concise and actionable. Focus on what matters most.");

  auto backend_name = d->llm_manager->activeBackendName();
  d->observer_session_id = d->agent_manager->createObserverSession(
      kObserverSystemPrompt, backend_name, d->current_session_id);

  if (d->observer_session_id < 0) {
    d->observer_btn->setChecked(false);
    AgentMessage err_msg;
    err_msg.role = QStringLiteral("system");
    err_msg.content = tr("Failed to create observer session.");
    d->conversation->addMessage(err_msg);
    return;
  }

  d->observer_enabled = true;
  d->observer_tool_call_count = 0;
  d->observer_recommendation_count = 0;
  d->observer_label->setText(tr("Observer: 0 recommendations"));
  d->observer_label->setVisible(true);

  AgentMessage sys_msg;
  sys_msg.role = QStringLiteral("system");
  sys_msg.content = tr("Observer mode enabled. The observer will review "
                       "the primary agent every %1 tool calls.")
                        .arg(d->observer_trigger_interval);
  d->conversation->addMessage(sys_msg);
}

void AgentExplorer::StopObserver(void) {
  if (d->observer_session_id >= 0) {
    d->agent_manager->cancelSession(d->observer_session_id);
    d->observer_session_id = -1;
  }
  d->observer_enabled = false;
  d->observer_label->setVisible(false);
  d->observer_btn->setChecked(false);
}

void AgentExplorer::OnObserverTriggered(int64_t observer_session_id) {
  if (observer_session_id != d->observer_session_id) {
    return;
  }

  AgentMessage sys_msg;
  sys_msg.role = QStringLiteral("system");
  sys_msg.content = tr("Observer triggered (review #%1).")
                        .arg(d->observer_recommendation_count + 1);
  d->conversation->addMessage(sys_msg);
}

void AgentExplorer::requestRecommendation(void) {
  if (d->config_panel->suggestionMode() == 0) {
    return;
  }

  auto *backend = d->llm_manager->activeBackend();
  if (!backend) {
    return;
  }

  // Gather last messages for context.
  auto all_msgs = d->agent_manager->sessionMessages(d->current_session_id);
  QStringList recent_lines;
  qsizetype start = qMax(qsizetype{0}, all_msgs.size() - qsizetype{10});
  for (qsizetype i = start; i < all_msgs.size(); ++i) {
    const auto &m = all_msgs[i];
    if (m.role == QStringLiteral("tool_call") ||
        m.role == QStringLiteral("tool_result")) {
      continue;
    }
    recent_lines.append(QStringLiteral("%1: %2").arg(m.role, m.content));
  }

  auto context_summary = d->recommender.context_summary;
  auto open_questions_str = d->recommender.open_questions.join(
      QStringLiteral("\n- "));
  if (!open_questions_str.isEmpty()) {
    open_questions_str.prepend(QStringLiteral("- "));
  }

  // Build task board summary.
  auto open_sheets = d->config_manager.LoadOpenSheets();
  QStringList sheet_summaries;
  for (const auto &sheet : open_sheets) {
    sheet_summaries.append(
        QStringLiteral("%1 (%2 rows)").arg(sheet.name).arg(sheet.cells.size()));
  }
  if (!sheet_summaries.isEmpty()) {
    if (!context_summary.isEmpty()) {
      context_summary.append(QStringLiteral("\n"));
    }
    context_summary.append(
        QStringLiteral("Open sheets: %1").arg(sheet_summaries.join(QStringLiteral(", "))));
  }

  static const QString kRecommenderPrompt = QString::fromUtf8(
      R"MX(You are a research assistant helping guide an analyst's conversation with an AI agent in a binary analysis IDE.

Recent conversation (last messages):
%1

Analysis context:
%2

Open questions:
%3

Respond with ONLY a JSON object (no markdown, no explanation):
{"suggestion":"the single best next question to ask","alternatives":["other good question 1","other good question 2"],"context_summary":"2-3 sentence summary of where analysis stands","open_questions":["unresolved question 1","unresolved question 2"]})MX");

  auto prompt_text = kRecommenderPrompt
      .arg(recent_lines.join(QStringLiteral("\n")))
      .arg(context_summary)
      .arg(open_questions_str);

  QVector<LLMMessage> messages;
  LLMMessage user_msg;
  user_msg.role = QStringLiteral("user");
  user_msg.content = prompt_text;
  messages.append(user_msg);

  LLMConfig config;
  config.temperature = 0.3;
  config.max_tokens = 512;
  config.model = d->llm_manager->backendConfig(
      d->llm_manager->activeBackendName(), QStringLiteral("model"));

  auto *thread = QThread::create([this, messages, config, backend] {
    auto response = backend->sendMessage(messages, {}, config);
    QMetaObject::invokeMethod(this, [this, response] {
      handleRecommendationResponse(response);
    }, Qt::QueuedConnection);
  });
  thread->start();
}

void AgentExplorer::handleRecommendationResponse(
    const LLMResponse &response) {
  if (!response.error.isEmpty()) {
    return;
  }

  d->recommender.suggestion_tokens +=
      response.prompt_tokens + response.completion_tokens;

  auto doc = QJsonDocument::fromJson(response.content.toUtf8());
  if (!doc.isObject()) {
    return;
  }

  auto obj = doc.object();
  auto suggestion = obj.value(QStringLiteral("suggestion")).toString();
  if (suggestion.isEmpty()) {
    return;
  }

  QStringList alternatives;
  auto alt_array = obj.value(QStringLiteral("alternatives")).toArray();
  for (const auto &val : alt_array) {
    auto s = val.toString();
    if (!s.isEmpty()) {
      alternatives.append(s);
    }
  }

  auto ctx = obj.value(QStringLiteral("context_summary")).toString();
  if (!ctx.isEmpty()) {
    d->recommender.context_summary = ctx;
  }

  QStringList questions;
  auto q_array = obj.value(QStringLiteral("open_questions")).toArray();
  for (const auto &val : q_array) {
    auto s = val.toString();
    if (!s.isEmpty()) {
      questions.append(s);
    }
  }
  if (!questions.isEmpty()) {
    d->recommender.open_questions = questions;
  }

  d->recommender.current_suggestion = suggestion;
  d->recommender.alternatives = alternatives;
  d->conversation->showSuggestion(suggestion, alternatives);
}

}  // namespace mx::gui
