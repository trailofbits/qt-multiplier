// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

namespace mx::gui {

class ILLMBackend;
class LLMManagerImpl;

class LLMManager Q_DECL_FINAL : public QObject {
  Q_OBJECT

 public:
  ~LLMManager(void);
  explicit LLMManager(QObject *parent = nullptr);

  // Supported backend type strings: "claude", "openai", "bedrock".
  static QStringList supportedBackendTypes(void);

  // Add a named backend of the given type. Returns false if name exists.
  bool addBackend(const QString &name, const QString &type);

  // Remove a named backend.
  void removeBackend(const QString &name);

  // Look up a backend by name. Returns nullptr if not found.
  ILLMBackend *backend(const QString &name) const;

  // All registered backend names.
  QStringList backendNames(void) const;

  // The type string for a named backend.
  QString backendType(const QString &name) const;

  // Per-backend key/value configuration (e.g. api_key, region, base_url).
  void setBackendConfig(const QString &name, const QString &key,
                        const QString &value);
  QString backendConfig(const QString &name, const QString &key) const;

  // Active backend selection.
  void setActiveBackend(const QString &name);
  QString activeBackendName(void) const;
  ILLMBackend *activeBackend(void) const;

  // Persistence via QSettings.
  void saveConfig(void) const;
  void loadConfig(void);

 signals:
  void backendsChanged(void);
  void activeBackendChanged(const QString &name);
  void backendConfigChanged(const QString &name, const QString &key);

 private:
  std::unique_ptr<LLMManagerImpl> d;
};

}  // namespace mx::gui
