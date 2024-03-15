// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Registry.h>

#include <QHash>
#include <QKeySequence>

namespace mx::gui {

struct Registry::PrivateData final {
  std::unique_ptr<QSettings> settings;
  QHash<QString, QHash<QString, KeyDescriptor>> module_map;
};

Registry::Ptr Registry::Create(const std::filesystem::path &path) {
  return Ptr(new Registry(path));
}

Registry::~Registry(void) {}

bool Registry::Set(const QString &module_name, const QString &key_name,
                   QVariant value) {
  if (!d->module_map.contains(module_name)) {
    qDebug() << "Module" << module_name << "is not defined";
    return false;
  }

  if (!value.isValid()) {
    qDebug() << "Invalid" << key_name << "setting passed to Module"
             << module_name;
    return false;
  }

  auto &key_map = d->module_map[module_name];
  if (!key_map.contains(key_name)) {
    qDebug() << "Trying to set an unknown key named" << key_name << "in module"
             << module_name;

    return false;
  }

  const auto &key_descriptor = key_map[key_name];
  if (!ValidateValueType(key_descriptor, value)) {
    qDebug() << "Invalid" << key_name << "setting type passed to Module"
             << module_name;
    return false;
  }

  d->settings->beginGroup(module_name);

  bool changed{false};
  if (auto old_value = d->settings->value(key_name); old_value.isValid()) {
    changed = (old_value != value);
  }

  if (changed && key_descriptor.opt_validator_callback.has_value()) {
    const auto &validator_callback =
        key_descriptor.opt_validator_callback.value();

    if (!validator_callback(*this, key_name, value)) {
      value = key_descriptor.default_value;
    }

    if (auto old_value = d->settings->value(key_name); old_value.isValid()) {
      changed = (old_value != value);
    }
  }

  if (changed) {
    d->settings->setValue(key_name, value);

    if (key_descriptor.opt_value_callback.has_value()) {
      const auto &value_callback = key_descriptor.opt_value_callback.value();
      value_callback(*this, key_name, value);
    }
  }

  d->settings->endGroup();
  return true;
}

QVariant Registry::Get(const QString &module_name,
                       const QString &key_name) const {

  if (!d->module_map.contains(module_name)) {
    qDebug() << "Module" << module_name << "is not defined";
    return false;
  }

  auto &key_map = d->module_map[module_name];
  if (!key_map.contains(key_name)) {
    qDebug() << "Trying to get an unknown key named" << key_name << "in module"
             << module_name;

    return false;
  }

  const auto &key_desc = key_map[key_name];

  d->settings->beginGroup(module_name);
  auto value = d->settings->value(key_name);
  d->settings->endGroup();

  switch (key_desc.type) {
    case Type::String:
    case Type::KeySequence: {
      value = value.toString();
      break;
    }

    case Type::Integer: {
      value = value.toUInt();
      break;
    }

    case Type::Boolean: {
      if (value.userType() == QMetaType::QString) {
        auto boolean_str = value.toString();
        if (boolean_str == "true") {
          value = true;

        } else if (boolean_str == "false") {
          value = false;

        } else {
          return false;
        }
      }

      value = value.toBool();
      break;
    }

    default: {
      value = {};
      break;
    }
  }

  return value;
}

Registry::Registry(const std::filesystem::path &path) : d(new PrivateData) {
  d->settings = CreateQSettings(path);
}

QHash<QString, QHash<QString, Registry::KeyDescriptor>>
Registry::ModuleMap(void) const {
  return d->module_map;
}

std::unique_ptr<QSettings>
Registry::CreateQSettings(const std::filesystem::path &path) {

  auto string_path = QString::fromStdString(path.generic_string());
  return std::make_unique<QSettings>(string_path, QSettings::Format::IniFormat);
}

bool Registry::ValidateValueType(const KeyDescriptor &key_desc,
                                 const QVariant &value) {

  auto value_is_valid{false};
  switch (key_desc.type) {
    case Type::String: {
      value_is_valid = value.userType() == QMetaType::QString;
      break;
    }

    case Type::Integer: {
      switch (value.userType()) {
        case QMetaType::Int:
        case QMetaType::UInt:
        case QMetaType::Long:
        case QMetaType::LongLong:
        case QMetaType::Short:
        case QMetaType::ULong:
        case QMetaType::ULongLong:
        case QMetaType::UShort: {
          value_is_valid = true;
          break;
        }

        default: break;
      }

      break;
    }

    case Type::Boolean: {
      if (value.userType() == QMetaType::Bool) {
        value_is_valid = true;

      } else if (value.userType() == QMetaType::QString) {
        auto string_value = value.toString();
        value_is_valid = (string_value == "true" || string_value == "false");
      }

      break;
    }

    case Type::KeySequence: {
      if (value.userType() == QMetaType::QString) {
        auto string_value = value.toString();

        QKeySequence key_sequence(string_value);
        if (key_sequence.isEmpty() || key_sequence.toString() != string_value) {
          break;
        }

        value_is_valid = true;
      }

      break;
    }
  }

  return value_is_valid;
}

void Registry::DefineModule(const QString &name, const bool &sync,
                            const KeyDescriptorList &key_desc_list) {

  if (d->module_map.contains(name)) {
    qDebug() << "Module" << name << "is already defined";
    return;
  }

  QHash<QString, KeyDescriptor> key_map;
  d->settings->beginGroup(name);

  for (const auto &key_desc : key_desc_list) {
    if (key_map.contains(key_desc.key_name)) {
      qDebug() << "KeyDescriptor" << key_desc.key_name << "in module" << name
               << "already exists";
      continue;
    }

    if (!key_desc.default_value.isValid()) {
      qDebug() << "KeyDescriptor" << key_desc.key_name << "in module" << name
               << "has no valid default value";
      continue;
    }

    key_map.insert(key_desc.key_name, key_desc);

    if (!d->settings->contains(key_desc.key_name)) {
      d->settings->setValue(key_desc.key_name, key_desc.default_value);

    } else {
      auto value = d->settings->value(key_desc.key_name);
      if (!ValidateValueType(key_desc, value)) {
        value = key_desc.default_value;
      }

      if (key_desc.opt_validator_callback.has_value()) {
        const auto &validator_callback =
            key_desc.opt_validator_callback.value();

        if (!validator_callback(*this, key_desc.key_name, value)) {
          value = key_desc.default_value;
        }
      }

      if (d->settings->value(key_desc.key_name) != value) {
        d->settings->setValue(key_desc.key_name, value);
      }
    }

    if (sync && key_desc.opt_value_callback.has_value()) {
      const auto &value_callback = key_desc.opt_value_callback.value();
      value_callback(*this, key_desc.key_name,
                     d->settings->value(key_desc.key_name));
    }
  }

  d->settings->endGroup();
  d->module_map.insert(name, std::move(key_map));

  emit SchemaChanged();
}

void Registry::SyncModule(const QString &name) {
  if (!d->module_map.contains(name)) {
    qDebug() << "Module" << name << "is not defined";
    return;
  }

  d->settings->beginGroup(name);

  const auto &key_map = d->module_map[name];
  for (const auto &key_desc : key_map) {
    if (!key_desc.opt_value_callback.has_value()) {
      continue;
    }

    const auto &value_callback = key_desc.opt_value_callback.value();
    value_callback(*this, key_desc.key_name,
                   d->settings->value(key_desc.key_name));
  }

  d->settings->endGroup();
}

}  // namespace mx::gui
