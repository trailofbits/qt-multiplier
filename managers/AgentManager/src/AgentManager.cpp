// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Managers/AgentManager.h>
#include <multiplier/GUI/Managers/LLMManager.h>

#include "AgentSession.h"
#include "AgentTool.h"
#include "AgentToolRegistry.h"

#include "tools/SpreadsheetTools.h"
#include "tools/DocumentTools.h"
#include "tools/NavigationTools.h"
#include "tools/SessionTools.h"

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
      d->max_iterations, this);

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
  connect(s, &AgentSession::sessionError, this,
          [this, session_id](const QString &error) {
            emit sessionError(session_id, error);
          });
  connect(s, &AgentSession::tokenUsageUpdated, this,
          [this, session_id](int prompt, int completion) {
            emit tokenUsageUpdated(session_id, prompt, completion);
          });

  d->sessions[session_id] = std::move(session);
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
  auto it = d->sessions.find(session_id);
  if (it != d->sessions.end()) {
    it->second->resume();
  }
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
  // Allocate persistent contexts owned by this manager.
  auto *ss_ctx = new SpreadsheetToolContext;
  ss_ctx->config = &config_manager;
  registerSpreadsheetTools(d->tool_registry, ss_ctx);

  auto *doc_ctx = new DocumentToolContext;
  doc_ctx->config = &config_manager;
  registerDocumentTools(d->tool_registry, doc_ctx);

  auto *nav_ctx = new NavigationToolContext;
  nav_ctx->config = &config_manager;
  registerNavigationTools(d->tool_registry, nav_ctx);

  auto *sess_ctx = new SessionToolContext;
  sess_ctx->config = &config_manager;
  registerSessionTools(d->tool_registry, sess_ctx);
}

void AgentManager::registerTool(std::unique_ptr<AgentTool> tool) {
  d->tool_registry.registerTool(std::move(tool));
}

}  // namespace mx::gui
