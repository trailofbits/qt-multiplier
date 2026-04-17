// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Managers/AgentManager.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/LLMManager.h>

#include "AgentSession.h"
#include "AgentTool.h"
#include "AgentToolRegistry.h"
#include "tools/SessionTools.h"

#include "tools/SpreadsheetTools.h"
#include "tools/TaskTools.h"
#include "tools/DocumentTools.h"
#include "tools/NavigationTools.h"
#include "tools/SessionTools.h"
#include "tools/ObserverTools.h"
#include "tools/PythonTools.h"

#include <QJsonDocument>
#include <QJsonObject>

#include <unordered_map>

namespace mx::gui {

struct QStringHash_AM {
  size_t operator()(const QString &s) const { return qHash(s); }
};

class AgentManagerImpl {
 public:
  LLMManager &llm_manager;
  AgentToolRegistry tool_registry;
  std::unordered_map<int64_t, std::unique_ptr<AgentSession>> sessions;
  int64_t next_session_id{1};
  int max_iterations{25};
  LLMConfig llm_config;

  // Observer mode: maps observer_session_id -> primary_session_id.
  std::unordered_map<int64_t, int64_t> observer_to_primary;
  // Observer tool registries (separate from primary, one per observer).
  std::unordered_map<int64_t, std::unique_ptr<AgentToolRegistry>>
      observer_registries;
  // Observer tool contexts (owned, one per observer).
  std::unordered_map<int64_t, std::unique_ptr<ObserverToolContext>>
      observer_contexts;
  ConfigManager *config_manager{nullptr};
  SessionToolContext *session_tool_ctx{nullptr};

  // Token tracking across all sessions.
  int accumulated_prompt_tokens{0};
  int accumulated_completion_tokens{0};

  explicit AgentManagerImpl(LLMManager &mgr) : llm_manager(mgr) {}
};

AgentManager::AgentManager(LLMManager &llm_manager, QObject *parent)
    : QObject(parent),
      d(std::make_unique<AgentManagerImpl>(llm_manager)) {}

AgentManager::~AgentManager(void) = default;

int64_t AgentManager::createSession(const QString &name,
                                    const QString &system_prompt,
                                    const QString &backend_name) {
  auto *backend = backend_name.isEmpty()
                      ? d->llm_manager.activeBackend()
                      : d->llm_manager.backend(backend_name);
  if (!backend) {
    return -1;
  }

  auto session_id = d->next_session_id++;
  auto session = std::make_unique<AgentSession>(
      session_id, backend, &d->tool_registry, d->llm_config, system_prompt,
      d->max_iterations, d->config_manager, this);

  // Forward session signals.
  auto *s = session.get();
  connect(s, &AgentSession::messageAdded, this,
          [this, session_id](const AgentMessage &msg) {
            emit messageAdded(session_id, msg);
          });
  connect(s, &AgentSession::toolCallStarted, this,
          [this, session_id](const QString &name, const QJsonObject &args) {
            emit toolCallStarted(session_id, name, args);
          });
  connect(s, &AgentSession::toolCallCompleted, this,
          [this, session_id](const QString &name, const QJsonObject &result,
                             int duration_ms) {
            emit toolCallCompleted(session_id, name, result, duration_ms);
          });
  connect(s, &AgentSession::sessionStarted, this,
          [this, session_id] { emit sessionStarted(session_id); });
  connect(s, &AgentSession::sessionPaused, this,
          [this, session_id] { emit sessionPaused(session_id); });
  connect(s, &AgentSession::sessionResumed, this,
          [this, session_id] { emit sessionResumed(session_id); });
  connect(s, &AgentSession::sessionCompleted, this,
          [this, session_id](const QString &summary) {
            emit sessionCompleted(session_id, summary);
          });
  connect(s, &AgentSession::sessionFinished, this,
          [this, session_id](const SessionResult &result) {
            emit sessionFinished(session_id, result);
          });
  connect(s, &AgentSession::sessionError, this,
          [this, session_id](const QString &error) {
            emit sessionError(session_id, error);
          });
  connect(s, &AgentSession::tokenUsageUpdated, this,
          [this, session_id](int prompt, int completion) {
            d->accumulated_prompt_tokens += prompt;
            d->accumulated_completion_tokens += completion;
            emit tokenUsageUpdated(session_id, prompt, completion);
          });

  d->sessions[session_id] = std::move(session);

  // Update session tool context so checkpoint/observation tools know the
  // current session.
  if (d->session_tool_ctx) {
    d->session_tool_ctx->current_session_id = session_id;
  }

  return session_id;
}

void AgentManager::sendMessage(int64_t session_id, const QString &text) {
  auto it = d->sessions.find(session_id);
  if (it != d->sessions.end()) {
    it->second->sendUserMessage(text);
  }
}

void AgentManager::pauseSession(int64_t session_id) {
  auto it = d->sessions.find(session_id);
  if (it != d->sessions.end()) {
    it->second->pause();
  }
}

void AgentManager::resumeSession(int64_t session_id) {
  // If the session is already in memory, just unpause it.
  auto it = d->sessions.find(session_id);
  if (it != d->sessions.end()) {
    it->second->resume();
    return;
  }

  // Reconstruct the session from the DB.
  if (!d->config_manager) {
    return;
  }

  auto system_prompt =
      d->config_manager->LoadAgentSessionSystemPrompt(session_id);

  auto *backend = d->llm_manager.activeBackend();
  if (!backend) {
    return;
  }

  // Assign a new in-memory session id that won't collide.
  // We keep the DB session_id for message loading but map it to the
  // in-memory key space.
  if (session_id >= d->next_session_id) {
    d->next_session_id = session_id + 1;
  }

  auto session = std::make_unique<AgentSession>(
      session_id, backend, &d->tool_registry, d->llm_config, system_prompt,
      d->max_iterations, d->config_manager, this);

  // Load persisted messages and populate the session history.
  auto db_messages = d->config_manager->LoadAgentMessages(session_id);
  QVector<AgentMessage> messages;
  messages.reserve(db_messages.size());
  for (const auto &info : db_messages) {
    AgentMessage msg;
    msg.message_id = info.message_id;
    msg.session_id = info.session_id;
    msg.role = info.role;
    msg.content = info.content;
    msg.tool_name = info.tool_name;
    msg.tool_call_id = info.tool_call_id;
    msg.token_count = info.token_count;
    msg.duration_ms = info.duration_ms;
    if (!info.tool_args.isEmpty()) {
      msg.tool_args =
          QJsonDocument::fromJson(info.tool_args.toUtf8()).object();
    }
    if (!info.tool_result.isEmpty()) {
      msg.tool_result =
          QJsonDocument::fromJson(info.tool_result.toUtf8()).object();
    }
    messages.append(std::move(msg));
  }
  session->loadMessages(messages);

  // Forward session signals (same wiring as createSession).
  auto *s = session.get();
  connect(s, &AgentSession::messageAdded, this,
          [this, session_id](const AgentMessage &msg) {
            emit messageAdded(session_id, msg);
          });
  connect(s, &AgentSession::toolCallStarted, this,
          [this, session_id](const QString &name, const QJsonObject &args) {
            emit toolCallStarted(session_id, name, args);
          });
  connect(s, &AgentSession::toolCallCompleted, this,
          [this, session_id](const QString &name, const QJsonObject &result,
                             int duration_ms) {
            emit toolCallCompleted(session_id, name, result, duration_ms);
          });
  connect(s, &AgentSession::sessionStarted, this,
          [this, session_id] { emit sessionStarted(session_id); });
  connect(s, &AgentSession::sessionPaused, this,
          [this, session_id] { emit sessionPaused(session_id); });
  connect(s, &AgentSession::sessionResumed, this,
          [this, session_id] { emit sessionResumed(session_id); });
  connect(s, &AgentSession::sessionCompleted, this,
          [this, session_id](const QString &summary) {
            emit sessionCompleted(session_id, summary);
          });
  connect(s, &AgentSession::sessionFinished, this,
          [this, session_id](const SessionResult &result) {
            emit sessionFinished(session_id, result);
          });
  connect(s, &AgentSession::sessionError, this,
          [this, session_id](const QString &error) {
            emit sessionError(session_id, error);
          });
  connect(s, &AgentSession::tokenUsageUpdated, this,
          [this, session_id](int prompt, int completion) {
            d->accumulated_prompt_tokens += prompt;
            d->accumulated_completion_tokens += completion;
            emit tokenUsageUpdated(session_id, prompt, completion);
          });

  d->sessions[session_id] = std::move(session);

  if (d->session_tool_ctx) {
    d->session_tool_ctx->current_session_id = session_id;
  }

  emit sessionResumed(session_id);
}

void AgentManager::cancelSession(int64_t session_id) {
  auto it = d->sessions.find(session_id);
  if (it != d->sessions.end()) {
    it->second->cancel();
  }
}

QVector<AgentMessage> AgentManager::sessionMessages(
    int64_t session_id) const {
  auto it = d->sessions.find(session_id);
  if (it != d->sessions.end()) {
    return it->second->messages();
  }
  return {};
}

bool AgentManager::isSessionRunning(int64_t session_id) const {
  auto it = d->sessions.find(session_id);
  if (it != d->sessions.end()) {
    return it->second->isRunning();
  }
  return false;
}

void AgentManager::setMaxIterations(int max) {
  d->max_iterations = max;
}

void AgentManager::setLLMConfig(const LLMConfig &config) {
  d->llm_config = config;
}

void AgentManager::registerBuiltinTools(ConfigManager &config_manager) {
  d->config_manager = &config_manager;

  // Allocate persistent contexts owned by this manager.
  auto *ss_ctx = new SpreadsheetToolContext;
  ss_ctx->config = &config_manager;
  registerSpreadsheetTools(d->tool_registry, ss_ctx);

  auto *task_ctx = new TaskToolContext;
  task_ctx->config = &config_manager;
  registerTaskTools(d->tool_registry, task_ctx);

  auto *doc_ctx = new DocumentToolContext;
  doc_ctx->config = &config_manager;
  registerDocumentTools(d->tool_registry, doc_ctx);

  auto *nav_ctx = new NavigationToolContext;
  nav_ctx->config = &config_manager;
  registerNavigationTools(d->tool_registry, nav_ctx);

  auto *sess_ctx = new SessionToolContext;
  sess_ctx->config = &config_manager;
  registerSessionTools(d->tool_registry, sess_ctx);
  d->session_tool_ctx = sess_ctx;

  auto *py_ctx = new PythonToolContext;
  py_ctx->config = &config_manager;
  registerPythonTools(d->tool_registry, py_ctx);
}

void AgentManager::registerTool(std::unique_ptr<AgentTool> tool) {
  d->tool_registry.registerTool(std::move(tool));
}

QVector<ToolDefinition> AgentManager::toolDefinitions(void) const {
  return d->tool_registry.allDefinitions();
}

int64_t AgentManager::createObserverSession(
    const QString &system_prompt, const QString &backend_name,
    int64_t primary_session_id, const QString &model_override) {
  auto *backend = backend_name.isEmpty()
                      ? d->llm_manager.activeBackend()
                      : d->llm_manager.backend(backend_name);
  if (!backend) {
    return -1;
  }

  // Verify primary session exists.
  if (d->sessions.find(primary_session_id) == d->sessions.end()) {
    return -1;
  }

  auto session_id = d->next_session_id++;

  // Create a dedicated tool registry for the observer with observer tools.
  auto registry = std::make_unique<AgentToolRegistry>();
  auto ctx = std::make_unique<ObserverToolContext>();
  ctx->config = d->config_manager;
  ctx->agent_manager = this;
  ctx->primary_session_id = primary_session_id;

  auto *ctx_ptr = ctx.get();
  registerObserverTools(*registry, ctx_ptr);

  auto observer_config = d->llm_config;
  if (!model_override.isEmpty()) {
    observer_config.model = model_override;
  }

  auto session = std::make_unique<AgentSession>(
      session_id, backend, registry.get(), observer_config, system_prompt,
      d->max_iterations, d->config_manager, this);

  // Forward session signals.
  auto *s = session.get();
  connect(s, &AgentSession::messageAdded, this,
          [this, session_id](const AgentMessage &msg) {
            emit messageAdded(session_id, msg);
          });
  connect(s, &AgentSession::toolCallStarted, this,
          [this, session_id](const QString &name, const QJsonObject &args) {
            emit toolCallStarted(session_id, name, args);
          });
  connect(s, &AgentSession::toolCallCompleted, this,
          [this, session_id](const QString &name, const QJsonObject &result,
                             int duration_ms) {
            emit toolCallCompleted(session_id, name, result, duration_ms);
          });
  connect(s, &AgentSession::sessionCompleted, this,
          [this, session_id](const QString &summary) {
            emit sessionCompleted(session_id, summary);
          });
  connect(s, &AgentSession::sessionFinished, this,
          [this, session_id](const SessionResult &result) {
            emit sessionFinished(session_id, result);
          });
  connect(s, &AgentSession::sessionError, this,
          [this, session_id](const QString &error) {
            emit sessionError(session_id, error);
          });
  connect(s, &AgentSession::tokenUsageUpdated, this,
          [this, session_id](int prompt, int completion) {
            d->accumulated_prompt_tokens += prompt;
            d->accumulated_completion_tokens += completion;
            emit tokenUsageUpdated(session_id, prompt, completion);
          });

  d->sessions[session_id] = std::move(session);
  d->observer_registries[session_id] = std::move(registry);
  d->observer_contexts[session_id] = std::move(ctx);
  d->observer_to_primary[session_id] = primary_session_id;

  return session_id;
}

void AgentManager::triggerObserver(int64_t observer_session_id) {
  auto it = d->sessions.find(observer_session_id);
  if (it == d->sessions.end()) {
    return;
  }

  // Don't trigger if already running.
  if (it->second->isRunning()) {
    return;
  }

  emit observerTriggered(observer_session_id);

  it->second->sendUserMessage(QStringLiteral(
      "Review the primary agent's recent activity using "
      "get_primary_session_context, then use observer_recommendation "
      "to record any findings."));
}

int64_t AgentManager::primarySessionId(
    int64_t observer_session_id) const {
  auto it = d->observer_to_primary.find(observer_session_id);
  if (it != d->observer_to_primary.end()) {
    return it->second;
  }
  return -1;
}

AgentManager::TokenSummary AgentManager::totalTokens(void) const {
  TokenSummary summary;
  summary.total_prompt_tokens = d->accumulated_prompt_tokens;
  summary.total_completion_tokens = d->accumulated_completion_tokens;
  return summary;
}

}  // namespace mx::gui
