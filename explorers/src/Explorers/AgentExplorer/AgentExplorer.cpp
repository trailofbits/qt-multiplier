// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Explorers/AgentExplorer.h>

#include <QAction>
#include <QCryptographicHash>
#include <QDateTime>
#include <QElapsedTimer>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QThread>
#include <QTimer>
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
#include <multiplier/GUI/Explorers/CodeExplorer.h>
#include <multiplier/AST/Decl.h>
#include <multiplier/Index.h>

#include "AgentConversationWidget.h"
#include "AgentConfigPanel.h"
#include "AgentDashboardWidget.h"
#include "AgentResourceLoader.h"
#include "AgentToolLogWidget.h"
#include "AgentSessionListWidget.h"

#include <multiplier/GUI/Widgets/SpreadsheetModel.h>

// Must be at global scope — Q_INIT_RESOURCE can't be inside a namespace.
static void initAgentResources(void) {
  Q_INIT_RESOURCE(AgentResources);
}

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
  IWindowWidget *dashboard_dock{nullptr};

  // Child widgets.
  AgentConversationWidget *conversation{nullptr};
  AgentConfigPanel *config_panel{nullptr};
  AgentToolLogWidget *tool_log{nullptr};
  AgentSessionListWidget *session_list{nullptr};
  AgentDashboardWidget *dashboard{nullptr};
  QTimer *dashboard_refresh_timer{nullptr};

  // Toolbar buttons.
  QPushButton *new_session_btn{nullptr};
  QPushButton *pause_btn{nullptr};
  QPushButton *stop_btn{nullptr};
  QPushButton *observer_btn{nullptr};
  QLabel *observer_label{nullptr};

  struct RecommenderState {
    QString context_summary;
    QStringList open_questions;
    QStringList benched_prompts;
    QString current_suggestion;
    QStringList alternatives;
    int suggestion_tokens{0};
  };
  RecommenderState recommender;

  struct CodeContext {
    QString code_summary;
    QStringList observations;
    QDateTime last_summarized;
    int views_since_summary{0};
    QString last_summarize_hash;
  };
  CodeContext code_context;
  CodeExplorer *code_explorer{nullptr};
  QTimer *context_refresh_timer{nullptr};

  // Cached prompt content loaded from resource documents.
  QString observer_prompt;
  QString code_summary_prompt;
  QString recommender_prompt;

  QString recommender_model;
  QString summarizer_model;
  QString observer_model;

  int64_t current_session_id{-1};
  bool paused{false};

  // Last root node ID from the primary session, for trigger edges.
  int64_t last_primary_root_node_id{-1};

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

  // Initialize Qt resources (must happen before loading resource files).
  initAgentResources();

  // Ensure all resource-backed documents exist in the database.
  ensureAllResourceDocuments(config_manager, {
    {QStringLiteral("Default Agent System Prompt"),
     QStringLiteral("prompt"),
     QStringLiteral(":/agent/prompts/system_prompt.md"), 10},
    {QStringLiteral("Observer System Prompt"),
     QStringLiteral("prompt"),
     QStringLiteral(":/agent/prompts/observer_prompt.md"), 0},
    {QStringLiteral("Code Summarizer Prompt"),
     QStringLiteral("prompt"),
     QStringLiteral(":/agent/prompts/code_summarizer_prompt.md"), 0},
    {QStringLiteral("Next Question Recommender Prompt"),
     QStringLiteral("prompt"),
     QStringLiteral(":/agent/prompts/recommender_prompt.md"), 0},
    {QStringLiteral("Multiplier Python API Reference"),
     QStringLiteral("reference"),
     QStringLiteral(":/agent/references/python_api.md"), 0},
    {QStringLiteral("Cell Content Format Reference"),
     QStringLiteral("reference"),
     QStringLiteral(":/agent/references/cell_format.md"), 0},
    {QStringLiteral("VR Observer Review Methodology"),
     QStringLiteral("skill"),
     QStringLiteral(":/agent/skills/vr_observer.md"), 0},
    {QStringLiteral("Fuzzer Harness Writing Guide"),
     QStringLiteral("skill"),
     QStringLiteral(":/agent/skills/fuzzer_harness.md"), 0},
  });

  // Notify Document Explorer that new documents may have been created.
  config_manager.NotifyExternalDocumentsChanged();

  // Cache the prompts that are used at runtime.
  d->observer_prompt = loadResource(
      QStringLiteral(":/agent/prompts/observer_prompt.md"));
  d->code_summary_prompt = loadResource(
      QStringLiteral(":/agent/prompts/code_summarizer_prompt.md"));
  d->recommender_prompt = loadResource(
      QStringLiteral(":/agent/prompts/recommender_prompt.md"));

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

  // Background code context refresh timer.
  d->context_refresh_timer = new QTimer(this);
  d->context_refresh_timer->setInterval(30000);
  connect(d->context_refresh_timer, &QTimer::timeout, this, [this] {
    if (d->code_explorer && d->code_context.views_since_summary >= 3) {
      summarizeViewedCode();
    }
  });
  d->context_refresh_timer->start();
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
  d->conversation = new AgentConversationWidget(theme_manager,
                                                  d->config_manager,
                                                  d->main_dock);
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

  // Bottom dock: dashboard.
  d->dashboard_dock = new IWindowWidget;
  d->dashboard_dock->setWindowTitle(tr("Agent Dashboard"));
  d->dashboard_dock->setContentsMargins(0, 0, 0, 0);

  auto *dashboard_layout = new QVBoxLayout(d->dashboard_dock);
  dashboard_layout->setContentsMargins(0, 0, 0, 0);

  d->dashboard = new AgentDashboardWidget(d->dashboard_dock);
  dashboard_layout->addWidget(d->dashboard);

  // Provide tool descriptions for dashboard tooltips.
  {
    QHash<QString, QString> tool_descs;
    auto defs = d->agent_manager->toolDefinitions();
    for (const auto &def : defs) {
      tool_descs.insert(def.name, def.description);
    }
    d->dashboard->setToolDescriptions(tool_descs);
  }

  {
    IWindowManager::DockConfig config;
    config.id = "com.trailofbits.dock.AgentDashboard";
    config.location = IWindowManager::DockLocation::Bottom;
    config.tabify = true;
    config.start_hidden = true;
    config.app_menu_location = {tr("Agent")};
    manager->AddDockWidget(d->dashboard_dock, config);
  }
}

void AgentExplorer::ConnectSignals(void) {
  // Debounced dashboard refresh: coalesces rapid tool calls into one update.
  d->dashboard_refresh_timer = new QTimer(this);
  d->dashboard_refresh_timer->setSingleShot(true);
  d->dashboard_refresh_timer->setInterval(500);  // 500ms debounce.
  connect(d->dashboard_refresh_timer, &QTimer::timeout, this, [this] {
    if (d->current_session_id >= 0 && d->dashboard) {
      d->dashboard->refresh(d->current_session_id, d->config_manager);
    }
  });
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

  // Store benched prompts from observer recommendations.
  connect(d->conversation, &AgentConversationWidget::promptBenched,
          this, [this](const QString &prompt) {
    if (!d->recommender.benched_prompts.contains(prompt)) {
      d->recommender.benched_prompts.append(prompt);
    }
  });

  // Create tasks directly in the task board for observer suggestions.
  connect(d->conversation, &AgentConversationWidget::scheduleTaskRequested,
          this, [this](const QString &desc) {
    auto &config = d->config_manager;

    // Find or create the task sheet.
    auto sheets = config.LoadOpenSheets();
    int sheet_id = -1;
    for (const auto &s : sheets) {
      if (s.role == QLatin1String("task_list")) {
        sheet_id = s.sheet_id;
        break;
      }
    }
    if (sheet_id < 0) {
      for (const auto &s : sheets) {
        if (s.name == QLatin1String("Task Board")) {
          sheet_id = s.sheet_id;
          break;
        }
      }
    }

    ConfigManager::SheetData sheet;
    if (sheet_id >= 0) {
      sheet = config.LoadSheetById(sheet_id);
    } else {
      // Create a new task sheet.
      sheet.name = QStringLiteral("Task Board");
      sheet.description = QStringLiteral("Agent task management board");
      sheet.role = QStringLiteral("task_list");
      sheet.key_column_index = 0;  // ID column.

      static const QStringList kCols = {
          QStringLiteral("ID"), QStringLiteral("Description"),
          QStringLiteral("Status"), QStringLiteral("Priority"),
          QStringLiteral("Entity"), QStringLiteral("Notes"),
          QStringLiteral("Done")};
      for (const auto &col_name : kCols) {
        ConfigManager::SheetColumnInfo ci;
        ci.name = col_name;
        sheet.columns.append(ci);
      }
    }

    // Determine next task ID.
    int max_num = 0;
    for (int r = 0; r < sheet.cells.size(); ++r) {
      if (0 < sheet.cells[r].size()) {
        auto v = SpreadsheetModel::value_from_json(sheet.cells[r][0]);
        auto id_text = SpreadsheetModel::display_text_for(v);
        if (id_text.startsWith(QLatin1String("T-"))) {
          bool ok = false;
          int num = id_text.mid(2).toInt(&ok);
          if (ok && num > max_num) max_num = num;
        }
      }
    }
    auto task_id = QStringLiteral("T-%1").arg(max_num + 1, 3, 10, QLatin1Char('0'));

    // Add row.
    int row = static_cast<int>(sheet.cells.size());
    int num_cols = static_cast<int>(sheet.columns.size());
    while (sheet.cells.size() <= row) {
      sheet.cells.append(QVector<QString>(num_cols));
    }
    auto &r = sheet.cells[row];
    while (r.size() < num_cols) r.append(QString());

    auto str_cell = [](const QString &s) {
      return SpreadsheetModel::value_to_json(QVariant(s));
    };
    auto bool_cell = [](bool b) {
      return SpreadsheetModel::value_to_json(QVariant(b));
    };

    r[0] = str_cell(task_id);          // ID
    r[1] = str_cell(desc);             // Description
    r[2] = str_cell(QStringLiteral("planned")); // Status
    r[3] = str_cell(QStringLiteral("medium"));  // Priority
    // Entity and Notes left empty.
    if (r.size() > 6) r[6] = bool_cell(false);  // Done

    config.SaveSheet(sheet);
    config.NotifyExternalSheetsChanged();
  });

  // Meta-observation: when the user follows an observer recommendation,
  // trigger the observer to evaluate the result.
  connect(d->conversation, &AgentConversationWidget::observerRecommendationFollowed,
          this, [this](const QString &rec) {
    if (d->observer_session_id < 0) return;

    auto meta_prompt = QStringLiteral(
        "The user just followed your recommendation: \"%1\"\n\n"
        "Review the primary agent's response and reflect:\n"
        "1. Did the agent address the recommendation effectively?\n"
        "2. Should any system prompts or analysis approaches be adjusted?\n"
        "3. What should the next step be?")
        .arg(rec);

    d->agent_manager->sendMessage(d->observer_session_id, meta_prompt);
  });

  // Navigate to entities when user double-clicks in tool result views.
  // Use the implicit preview action to open in the code preview widget.
  auto navigate_trigger = d->config_manager.ActionManager().Find(
      "com.trailofbits.action.OpenEntityPreview");
  connect(d->conversation, &AgentConversationWidget::navigateToEntity,
          this, [this, navigate_trigger](uint64_t entity_id) mutable {
    auto vent = d->config_manager.Index().entity(
        mx::EntityId(static_cast<mx::RawEntityId>(entity_id)));
    if (!std::holds_alternative<mx::NotAnEntity>(vent)) {
      if (std::holds_alternative<mx::Decl>(vent)) {
        auto decl = std::get<mx::Decl>(vent);
        if (auto def = decl.definition()) {
          vent = def.value();
        }
      }
      navigate_trigger.Trigger(QVariant::fromValue(vent));
    }
  });

  // Open documents when user clicks a document link.
  auto doc_trigger = d->config_manager.ActionManager().Find(
      "com.trailofbits.action.OpenDocument");
  connect(d->conversation, &AgentConversationWidget::openDocument,
          this, [doc_trigger](int doc_id) mutable {
    doc_trigger.Trigger(QVariant(doc_id));
  });

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

  // Show activity indicator.
  d->conversation->setStatus(tr("Agent is thinking..."));

  // Send to agent. The AgentManager will emit messageAdded, which
  // triggers OnMessageAdded to display it in the conversation.
  d->agent_manager->sendMessage(d->current_session_id, text);

  // Capture the root cost node for trigger edge tracking. The root node
  // (user_message with no parent) is created synchronously in sendUserMessage
  // before the worker thread starts.
  auto cost_nodes = d->config_manager.LoadCostNodes(d->current_session_id);
  for (const auto &node : cost_nodes) {
    if (node.parent_node_id < 0 &&
        node.node_type == QStringLiteral("user_message")) {
      d->last_primary_root_node_id = node.node_id;
    }
  }
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
  d->conversation->clearStatus();
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
  // Show in conversation only for the primary session.
  if (session_id == d->current_session_id) {
    d->conversation->addMessage(msg);

    // Clear status when the agent produces a text response.
    if (msg.role == QStringLiteral("assistant")) {
      d->conversation->clearStatus();
    }
  }

  // Persist messages for both primary and observer sessions.
  // Skip user messages for the primary session (already persisted in
  // OnSendMessage). Observer user messages are synthetic triggers, persist them.
  if (session_id != d->current_session_id &&
      session_id != d->observer_session_id) {
    return;
  }

  bool skip_user = (session_id == d->current_session_id &&
                    msg.role == QStringLiteral("user"));
  if (!skip_user) {
    QString tool_args_str;
    if (!msg.tool_args.isEmpty()) {
      tool_args_str = QString::fromUtf8(
          QJsonDocument(msg.tool_args).toJson(QJsonDocument::Compact));
    }
    QString tool_result_str;
    if (!msg.tool_result.isEmpty()) {
      tool_result_str = QString::fromUtf8(
          QJsonDocument(msg.tool_result).toJson(QJsonDocument::Compact));
    }
    d->config_manager.SaveAgentMessage(
        session_id, msg.role, msg.content,
        msg.tool_name, msg.tool_call_id,
        tool_args_str, tool_result_str,
        msg.token_count, msg.duration_ms);
  }
}

void AgentExplorer::OnTokenUsageUpdated(int64_t session_id,
                                         int prompt_tokens,
                                         int completion_tokens) {
  // Track tokens for both primary and observer sessions.
  if (session_id != d->current_session_id &&
      session_id != d->observer_session_id) {
    return;
  }

  // Show cumulative tokens across all related sessions.
  auto totals = d->agent_manager->totalTokens();
  auto summary = d->config_manager.LoadCostSummary(d->current_session_id);
  d->conversation->updateTokens(totals.total_prompt_tokens,
                                totals.total_completion_tokens,
                                summary.total_cost_usd);
  d->config_manager.UpdateAgentSessionTokens(
      session_id, prompt_tokens, completion_tokens);

  // Trigger debounced dashboard refresh on LLM call completion.
  d->dashboard_refresh_timer->start();
}

void AgentExplorer::OnSessionCompleted(int64_t session_id,
                                        const QString &summary) {
  if (session_id != d->current_session_id) {
    return;
  }

  d->conversation->clearStatus();

  AgentMessage msg;
  msg.role = QStringLiteral("system");
  msg.content = tr("Session completed. %1").arg(summary);
  d->conversation->addMessage(msg);

  d->config_manager.UpdateAgentSessionStatus(
      session_id, QStringLiteral("completed"));
  d->session_list->refresh();
  d->dashboard->refresh(session_id, d->config_manager);
}

void AgentExplorer::OnSessionFinished(int64_t session_id,
                                       const SessionResult &result) {
  if (session_id != d->current_session_id) {
    return;
  }

  d->conversation->clearStatus();

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
  d->dashboard->refresh(session_id, d->config_manager);
}

void AgentExplorer::OnSessionError(int64_t session_id,
                                    const QString &error) {
  if (session_id != d->current_session_id) {
    return;
  }

  d->conversation->clearStatus();

  AgentMessage msg;
  msg.role = QStringLiteral("system");
  msg.content = tr("Error: %1").arg(error);
  d->conversation->addMessage(msg);
}

void AgentExplorer::OnSessionSelected(int64_t session_id) {
  LoadSession(session_id);
}

void AgentExplorer::OnSessionResumeRequested(int64_t session_id) {
  // Stop any existing session and observer.
  StopObserver();
  if (d->current_session_id >= 0) {
    d->agent_manager->cancelSession(d->current_session_id);
  }

  // Load conversation history into the UI.
  LoadSession(session_id);

  // Reconstruct the in-memory session from DB state.
  d->agent_manager->resumeSession(session_id);

  d->config_manager.UpdateAgentSessionStatus(
      session_id, QStringLiteral("active"));
  d->paused = false;
  d->pause_btn->setText(tr("Pause"));
  d->observer_tool_call_count = 0;
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

  // Enter-to-send preference.
  d->conversation->setEnterToSend(d->config_panel->enterToSend());

  // Per-role model overrides.
  d->recommender_model = d->config_panel->recommenderModel();
  d->summarizer_model = d->config_panel->summarizerModel();
  d->observer_model = d->config_panel->observerModel();
}

void AgentExplorer::OnToolCallStarted(int64_t session_id, const QString &name,
                                       const QJsonObject &args) {
  if (session_id != d->current_session_id) {
    return;
  }
  d->tool_log->onToolCallStarted(session_id, name, args);
  d->conversation->setStatus(tr("Calling: %1...").arg(name));
  // The tool_call message is already emitted via messageAdded and handled
  // in OnMessageAdded (stashed for later merging with tool_result).
}

void AgentExplorer::OnToolCallCompleted(int64_t session_id,
                                         const QString &name,
                                         const QJsonObject &result,
                                         int duration_ms) {
  if (session_id == d->current_session_id) {
    d->tool_log->onToolCallCompleted(session_id, name, result, duration_ms);
    d->conversation->setStatus(tr("Agent is thinking..."));

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

      // Show the recommendation inline in the conversation.
      auto rec_text = result[QStringLiteral("recommendation")].toString();
      QStringList prompts;
      auto prompts_arr =
          result[QStringLiteral("suggested_prompts")].toArray();
      for (const auto &v : prompts_arr) {
        prompts.append(v.toString());
      }
      if (!rec_text.isEmpty()) {
        d->conversation->showObserverRecommendation(rec_text, prompts);
      }
    }
  }

  // Trigger debounced dashboard refresh on every tool call.
  d->dashboard_refresh_timer->start();
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
    if (!info.tool_args.isEmpty()) {
      msg.tool_args = QJsonDocument::fromJson(
          info.tool_args.toUtf8()).object();
    }
    if (!info.tool_result.isEmpty()) {
      msg.tool_result = QJsonDocument::fromJson(
          info.tool_result.toUtf8()).object();
    }
    d->conversation->addMessage(msg);
  }

  // Refresh the dashboard for the loaded session.
  d->dashboard->refresh(session_id, d->config_manager);
}

void AgentExplorer::OnToggleObserver(bool checked) {
  if (checked) {
    StartObserver();
  } else {
    StopObserver();
  }

  bool enabled = d->observer_btn->isChecked();
  d->observer_btn->setText(enabled ? tr("Observer: ON") : tr("Observer"));
  d->observer_btn->setStyleSheet(enabled
      ? QStringLiteral("QPushButton { font-weight: bold; }")
      : QString());
}

void AgentExplorer::StartObserver(void) {
  if (d->current_session_id < 0) {
    d->observer_btn->setChecked(false);
    return;
  }

  if (d->observer_session_id >= 0) {
    return;  // Already running.
  }

  auto backend_name = d->llm_manager->activeBackendName();
  d->observer_session_id = d->agent_manager->createObserverSession(
      d->observer_prompt, backend_name, d->current_session_id,
      d->observer_model);

  if (d->observer_session_id < 0) {
    d->observer_btn->setChecked(false);
    AgentMessage obs_err;
    obs_err.role = QStringLiteral("system");
    obs_err.content = tr("Failed to create observer session.");
    d->conversation->addMessage(obs_err);
    return;
  }

  // Persist the observer session.
  {
    auto *obs_backend = d->llm_manager->activeBackend();
    QString obs_model = obs_backend ? obs_backend->name() : QString();
    d->config_manager.CreateAgentSession(
        tr("Observer"), d->observer_prompt, backend_name, obs_model,
        d->current_session_id);
  }

  d->observer_enabled = true;
  d->observer_tool_call_count = 0;
  d->observer_recommendation_count = 0;
  d->observer_label->setText(tr("Observer: 0 recommendations"));
  d->observer_label->setVisible(true);

  AgentMessage sys_msg;
  sys_msg.role = QStringLiteral("system");
  sys_msg.content = tr("Observer enabled: a secondary agent will periodically "
                       "review your session and write recommendations to an "
                       "'Observer Notes' document.");
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

  AgentMessage sys_msg;
  sys_msg.role = QStringLiteral("system");
  sys_msg.content = tr("Observer disabled.");
  d->conversation->addMessage(sys_msg);
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

void AgentExplorer::setCodeExplorer(CodeExplorer *explorer) {
  d->code_explorer = explorer;
}

void AgentExplorer::summarizeViewedCode(void) {
  if (!d->code_explorer) return;

  auto viewed = d->code_explorer->recentlyViewedCode(10);
  if (viewed.isEmpty()) return;

  auto *backend = d->llm_manager->activeBackend();
  if (!backend) return;

  // Build code snippets string.
  QString code_snippets;
  for (const auto &vc : viewed) {
    code_snippets += QStringLiteral("--- %1 (%2) [%3] ---\n")
        .arg(vc.label, vc.kind, vc.file_path);
    if (!vc.code_snippet.isEmpty()) {
      code_snippets += vc.code_snippet;
      code_snippets += QStringLiteral("\n");
    }
    code_snippets += QStringLiteral("\n");
  }

  // Skip if the viewed code hasn't changed since the last summarization.
  auto snippets_hash = QString::fromLatin1(
      QCryptographicHash::hash(code_snippets.toUtf8(),
                               QCryptographicHash::Md5).toHex());
  if (snippets_hash == d->code_context.last_summarize_hash) {
    d->code_context.views_since_summary = 0;
    return;
  }
  d->code_context.last_summarize_hash = snippets_hash;

  auto prompt_text = d->code_summary_prompt.arg(code_snippets);

  QVector<LLMMessage> messages;
  LLMMessage user_msg;
  user_msg.role = QStringLiteral("user");
  user_msg.content = prompt_text;
  messages.append(user_msg);

  auto primary_model = d->llm_manager->backendConfig(
      d->llm_manager->activeBackendName(), QStringLiteral("model"));

  LLMConfig config;
  config.temperature = 0.2;
  config.max_tokens = 512;
  config.model = d->summarizer_model.isEmpty()
      ? primary_model : d->summarizer_model;

  // Create a cost node for the summarizer call and a trigger edge from
  // the session's root node.
  int64_t summarizer_node_id = -1;
  if (d->current_session_id >= 0) {
    summarizer_node_id = d->config_manager.CreateCostNode(
        d->current_session_id, -1, QStringLiteral("summarizer"),
        {}, config.model);
    if (summarizer_node_id >= 0 && d->last_primary_root_node_id >= 0) {
      d->config_manager.CreateCostEdge(
          d->last_primary_root_node_id, summarizer_node_id,
          QStringLiteral("trigger"));
    }
  }

  auto *thread = QThread::create(
      [this, messages, config, backend, summarizer_node_id] {
    QElapsedTimer timer;
    timer.start();
    auto response = backend->sendMessage(messages, {}, config);
    auto duration_ms = static_cast<int>(timer.elapsed());
    if (summarizer_node_id >= 0) {
      QMetaObject::invokeMethod(&d->config_manager, [&] {
        d->config_manager.CompleteCostNode(
            summarizer_node_id, response.prompt_tokens,
            response.completion_tokens, duration_ms);
      }, Qt::BlockingQueuedConnection);
    }
    QMetaObject::invokeMethod(this, [this, response] {
      handleCodeSummaryResponse(response);
    }, Qt::QueuedConnection);
  });
  thread->start();
}

void AgentExplorer::handleCodeSummaryResponse(const LLMResponse &response) {
  if (!response.error.isEmpty()) {
    AgentMessage err_msg;
    err_msg.role = QStringLiteral("system");
    err_msg.content = tr("Code summarizer error (model: %1): %2")
        .arg(d->summarizer_model.isEmpty()
                 ? QStringLiteral("default")
                 : d->summarizer_model,
             response.error);
    d->conversation->addMessage(err_msg);
    return;
  }

  auto doc = QJsonDocument::fromJson(response.content.toUtf8());
  if (!doc.isObject()) return;

  auto obj = doc.object();

  auto summary = obj.value(QStringLiteral("code_summary")).toString();
  if (!summary.isEmpty()) {
    d->code_context.code_summary = summary;
  }

  QStringList observations;
  auto obs_array = obj.value(QStringLiteral("observations")).toArray();
  for (const auto &val : obs_array) {
    auto s = val.toString();
    if (!s.isEmpty()) {
      observations.append(s);
    }
  }
  if (!observations.isEmpty()) {
    d->code_context.observations = observations;
  }

  d->code_context.last_summarized = QDateTime::currentDateTime();
  d->code_context.views_since_summary = 0;
}

void AgentExplorer::requestRecommendation(void) {
  if (d->config_panel->suggestionMode() == 0) {
    return;
  }

  auto *backend = d->llm_manager->activeBackend();
  if (!backend) {
    return;
  }

  // Bump view count for code context tracking.
  d->code_context.views_since_summary++;

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

  // Append code context if available.
  QString code_context_str;
  if (!d->code_context.code_summary.isEmpty()) {
    code_context_str = QStringLiteral("\nCode the analyst is examining:\n%1")
        .arg(d->code_context.code_summary);
    if (!d->code_context.observations.isEmpty()) {
      code_context_str += QStringLiteral("\n\nSecurity-relevant observations:\n");
      for (const auto &obs : d->code_context.observations) {
        code_context_str += QStringLiteral("- %1\n").arg(obs);
      }
    }
  }

  // Append recent observer recommendations if observer session exists.
  QString observer_recs_str;
  if (d->observer_session_id >= 0) {
    auto obs_msgs = d->agent_manager->sessionMessages(d->observer_session_id);
    QStringList obs_recs;
    for (auto it = obs_msgs.rbegin();
         it != obs_msgs.rend() && obs_recs.size() < 5; ++it) {
      if (it->role == QStringLiteral("tool_call") &&
          it->tool_name == QStringLiteral("observer_recommendation")) {
        auto rec = it->tool_args.value(
            QStringLiteral("recommendation")).toString();
        if (!rec.isEmpty()) {
          obs_recs.prepend(rec);
        }
      }
    }
    if (!obs_recs.isEmpty()) {
      observer_recs_str = QStringLiteral(
          "\nRecent observer recommendations:\n");
      for (const auto &r : obs_recs) {
        observer_recs_str += QStringLiteral("- %1\n").arg(r);
      }
    }
  }

  // Append benched prompts if any.
  QString benched_str;
  if (!d->recommender.benched_prompts.isEmpty()) {
    benched_str = QStringLiteral(
        "\nBenched prompts (from observer, not yet executed):\n");
    for (const auto &bp : d->recommender.benched_prompts) {
      benched_str += QStringLiteral("- %1\n").arg(bp);
    }
  }

  auto prompt_text = d->recommender_prompt
      .arg(recent_lines.join(QStringLiteral("\n")))
      .arg(context_summary)
      .arg(open_questions_str)
      .arg(code_context_str + observer_recs_str + benched_str);

  QVector<LLMMessage> messages;
  LLMMessage user_msg;
  user_msg.role = QStringLiteral("user");
  user_msg.content = prompt_text;
  messages.append(user_msg);

  auto primary_model = d->llm_manager->backendConfig(
      d->llm_manager->activeBackendName(), QStringLiteral("model"));

  LLMConfig config;
  config.temperature = 0.3;
  config.max_tokens = 512;
  config.model = d->recommender_model.isEmpty()
      ? primary_model : d->recommender_model;

  // Create a cost node for the recommender call and a trigger edge from
  // the session's root node.
  int64_t recommender_node_id = -1;
  if (d->current_session_id >= 0) {
    recommender_node_id = d->config_manager.CreateCostNode(
        d->current_session_id, -1, QStringLiteral("recommender"),
        {}, config.model);
    if (recommender_node_id >= 0 && d->last_primary_root_node_id >= 0) {
      d->config_manager.CreateCostEdge(
          d->last_primary_root_node_id, recommender_node_id,
          QStringLiteral("trigger"));
    }
  }

  auto *thread = QThread::create(
      [this, messages, config, backend, recommender_node_id] {
    QElapsedTimer timer;
    timer.start();
    auto response = backend->sendMessage(messages, {}, config);
    auto duration_ms = static_cast<int>(timer.elapsed());
    if (recommender_node_id >= 0) {
      QMetaObject::invokeMethod(&d->config_manager, [&] {
        d->config_manager.CompleteCostNode(
            recommender_node_id, response.prompt_tokens,
            response.completion_tokens, duration_ms);
      }, Qt::BlockingQueuedConnection);
    }
    QMetaObject::invokeMethod(this, [this, response] {
      handleRecommendationResponse(response);
    }, Qt::QueuedConnection);
  });
  thread->start();
}

void AgentExplorer::handleRecommendationResponse(
    const LLMResponse &response) {
  if (!response.error.isEmpty()) {
    AgentMessage err_msg;
    err_msg.role = QStringLiteral("system");
    err_msg.content = tr("Recommender error (model: %1): %2")
        .arg(d->recommender_model.isEmpty()
                 ? QStringLiteral("default")
                 : d->recommender_model,
             response.error);
    d->conversation->addMessage(err_msg);
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
