// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Managers/LLMManager.h>

#include "BedrockBackend.h"
#include "ClaudeBackend.h"
#include "OpenAICompatBackend.h"

#include <QSettings>

#include <memory>
#include <unordered_map>

namespace mx::gui {

struct QStringHash {
  size_t operator()(const QString &s) const { return qHash(s); }
};

struct BackendEntry {
  std::unique_ptr<ILLMBackend> backend;
  QString type;
  QMap<QString, QString> config;
};

class LLMManagerImpl {
 public:
  std::unordered_map<QString, std::unique_ptr<BackendEntry>, QStringHash>
      backends;
  QString active_backend_name;
};

LLMManager::LLMManager(QObject *parent)
    : QObject(parent),
      d(std::make_unique<LLMManagerImpl>()) {}

LLMManager::~LLMManager(void) = default;

QStringList LLMManager::supportedBackendTypes(void) {
  return {QStringLiteral("claude"), QStringLiteral("openai"),
          QStringLiteral("bedrock")};
}

bool LLMManager::addBackend(const QString &name, const QString &type) {
  if (d->backends.count(name)) {
    return false;
  }

  auto entry = std::make_unique<BackendEntry>();
  entry->type = type;

  if (type == QStringLiteral("claude")) {
    entry->backend = std::make_unique<ClaudeBackend>(this);
  } else if (type == QStringLiteral("openai")) {
    entry->backend = std::make_unique<OpenAICompatBackend>(this);
  } else if (type == QStringLiteral("bedrock")) {
    entry->backend = std::make_unique<BedrockBackend>(this);
  } else {
    return false;
  }

  d->backends[name] = std::move(entry);
  emit backendsChanged();
  return true;
}

void LLMManager::removeBackend(const QString &name) {
  auto it = d->backends.find(name);
  if (it == d->backends.end()) {
    return;
  }

  d->backends.erase(it);

  if (d->active_backend_name == name) {
    d->active_backend_name.clear();
    emit activeBackendChanged(d->active_backend_name);
  }

  emit backendsChanged();
}

ILLMBackend *LLMManager::backend(const QString &name) const {
  auto it = d->backends.find(name);
  if (it == d->backends.end()) {
    return nullptr;
  }
  return it->second->backend.get();
}

QStringList LLMManager::backendNames(void) const {
  QStringList names;
  for (const auto &[name, _] : d->backends) {
    names.append(name);
  }
  return names;
}

QString LLMManager::backendType(const QString &name) const {
  auto it = d->backends.find(name);
  if (it == d->backends.end()) {
    return {};
  }
  return it->second->type;
}

void LLMManager::setBackendConfig(const QString &name, const QString &key,
                                  const QString &value) {
  auto it = d->backends.find(name);
  if (it == d->backends.end()) {
    return;
  }

  it->second->config[key] = value;

  // Propagate to the backend implementation.
  auto type = it->second->type;
  if (type == QStringLiteral("claude")) {
    static_cast<ClaudeBackend *>(it->second->backend.get())
        ->setConfig(key, value);
  } else if (type == QStringLiteral("openai")) {
    static_cast<OpenAICompatBackend *>(it->second->backend.get())
        ->setConfig(key, value);
  } else if (type == QStringLiteral("bedrock")) {
    static_cast<BedrockBackend *>(it->second->backend.get())
        ->setConfig(key, value);
  }

  emit backendConfigChanged(name, key);
}

QString LLMManager::backendConfig(const QString &name,
                                  const QString &key) const {
  auto it = d->backends.find(name);
  if (it == d->backends.end()) {
    return {};
  }
  return it->second->config.value(key);
}

void LLMManager::setActiveBackend(const QString &name) {
  if (d->active_backend_name == name) {
    return;
  }
  d->active_backend_name = name;
  emit activeBackendChanged(name);
}

QString LLMManager::activeBackendName(void) const {
  return d->active_backend_name;
}

ILLMBackend *LLMManager::activeBackend(void) const {
  return backend(d->active_backend_name);
}

void LLMManager::setApiKeyForType(const QString &type, const QString &key) {
  // Apply to all backends of this type.
  for (auto &[name, entry] : d->backends) {
    if (entry->type == type) {
      entry->config[QStringLiteral("api_key")] = key;
    }
  }

  // Store separately so it persists even for backends not yet created.
  QSettings settings;
  settings.beginGroup(QStringLiteral("LLMManager_ApiKeys"));
  settings.setValue(type, key);
  settings.endGroup();
  settings.sync();
}

QString LLMManager::apiKeyForType(const QString &type) const {
  // Check in-memory backends first.
  for (const auto &[name, entry] : d->backends) {
    if (entry->type == type) {
      auto key = entry->config.value(QStringLiteral("api_key"));
      if (!key.isEmpty()) return key;
    }
  }

  // Fall back to stored per-type key.
  QSettings settings;
  settings.beginGroup(QStringLiteral("LLMManager_ApiKeys"));
  auto key = settings.value(type).toString();
  settings.endGroup();
  return key;
}

void LLMManager::clearAllApiKeys(void) {
  // Clear from all backends.
  for (auto &[name, entry] : d->backends) {
    entry->config.remove(QStringLiteral("api_key"));
  }

  // Clear stored per-type keys.
  QSettings settings;
  settings.remove(QStringLiteral("LLMManager_ApiKeys"));
  settings.sync();

  // Re-save backend configs without keys.
  saveConfig();
}

void LLMManager::saveConfig(void) const {
  QSettings settings;
  settings.beginGroup(QStringLiteral("LLMManager"));

  // Clear old entries.
  settings.remove(QString());

  settings.setValue(QStringLiteral("active_backend"),
                   d->active_backend_name);

  settings.beginWriteArray(QStringLiteral("backends"));
  int idx = 0;
  for (const auto &[name, entry] : d->backends) {
    settings.setArrayIndex(idx++);
    settings.setValue(QStringLiteral("name"), name);
    settings.setValue(QStringLiteral("type"), entry->type);

    settings.beginGroup(QStringLiteral("config"));
    for (auto it = entry->config.begin(); it != entry->config.end(); ++it) {
      settings.setValue(it.key(), it.value());
    }
    settings.endGroup();
  }
  settings.endArray();

  settings.endGroup();
}

void LLMManager::loadConfig(void) {
  QSettings settings;
  settings.beginGroup(QStringLiteral("LLMManager"));

  auto active = settings.value(QStringLiteral("active_backend")).toString();

  int count = settings.beginReadArray(QStringLiteral("backends"));
  for (int i = 0; i < count; ++i) {
    settings.setArrayIndex(i);
    auto name = settings.value(QStringLiteral("name")).toString();
    auto type = settings.value(QStringLiteral("type")).toString();

    if (name.isEmpty() || type.isEmpty()) {
      continue;
    }

    addBackend(name, type);

    settings.beginGroup(QStringLiteral("config"));
    for (const auto &key : settings.childKeys()) {
      setBackendConfig(name, key, settings.value(key).toString());
    }
    settings.endGroup();
  }
  settings.endArray();

  if (!active.isEmpty() && d->backends.count(active)) {
    setActiveBackend(active);
  }

  settings.endGroup();

  // Restore per-type API keys into any backend that doesn't have one.
  QSettings key_settings;
  key_settings.beginGroup(QStringLiteral("LLMManager_ApiKeys"));
  for (auto &[name, entry] : d->backends) {
    if (entry->config.value(QStringLiteral("api_key")).isEmpty()) {
      auto type_key = key_settings.value(entry->type).toString();
      if (!type_key.isEmpty()) {
        entry->config[QStringLiteral("api_key")] = type_key;
      }
    }
  }
  key_settings.endGroup();
}

}  // namespace mx::gui
