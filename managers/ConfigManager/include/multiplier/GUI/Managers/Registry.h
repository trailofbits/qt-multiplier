// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <memory>
#include <filesystem>
#include <functional>
#include <optional>

#include <QString>
#include <QObject>
#include <QVariant>
#include <QSettings>
#include <QHash>

namespace mx::gui {

class Registry final : public QObject {
  Q_OBJECT

 public:
  using Ptr = std::unique_ptr<Registry>;

  static Ptr Create(const std::filesystem::path &path);
  virtual ~Registry();

  enum class Type {
    String,
    Integer,
    Boolean,
    KeySequence,
  };

  struct KeyInformation final {
    Type type{Type::String};
    QString description;
  };

  using KeyMap = QHash<QString, QHash<QString, KeyInformation>>;

  KeyMap GetKeyMap() const;

  bool Set(const QString &module_name, const QString &key_name, QVariant value);

  QVariant Get(const QString &module_name, const QString &key_name) const;

  struct KeyDescriptor final {
    using ValidatorCallback = std::function<bool(
        const Registry &registry, const QString &key_name, QVariant &value)>;

    using ValueCallback =
        std::function<void(const Registry &registry, const QString &key_name,
                           const QVariant &value)>;

    Type type{Type::String};
    QString name;
    QString description;

    QVariant default_value;
    std::optional<ValidatorCallback> opt_validator_callback;
    std::optional<ValueCallback> opt_value_callback;
  };

  using KeyDescriptorList = std::vector<KeyDescriptor>;

  void DefineModule(const QString &name, const bool &sync,
                    const KeyDescriptorList &key_desc_list);

  void SyncModule(const QString &name);

  Registry(const Registry &) = delete;
  Registry &operator=(const Registry &) = delete;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  Registry(const std::filesystem::path &path);

  static std::unique_ptr<QSettings>
  CreateQSettings(const std::filesystem::path &path);

  static bool ValidateValueType(const KeyDescriptor &key_desc,
                                const QVariant &value);

 signals:
  void SchemaChanged();
};

}  // namespace mx::gui
