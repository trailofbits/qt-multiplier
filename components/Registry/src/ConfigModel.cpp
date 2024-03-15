// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "ConfigModel.h"
#include "RegistryInternals.h"

#include <QAbstractItemModelTester>

#include <unordered_map>

namespace mx::gui {

namespace {

struct Node final {
  struct InternalRootData final {
    std::vector<quintptr> child_node_id_list;
  };

  struct ModuleRootData final {
    QString name;
    std::vector<quintptr> child_node_id_list;
  };

  struct ModuleKeyData final {
    QString key_name;
    QString localized_key_name;
    QString description;
    Registry::Type type{Registry::Type::String};
    quintptr parent_node_id{};
  };

  quintptr node_id{};
  int parent_row{};
  int row{};

  std::variant<std::monostate, InternalRootData, ModuleRootData, ModuleKeyData>
      data_var;
};

using NodeMap = std::unordered_map<quintptr, Node>;

NodeMap ImportRegistry(const Registry &registry) {
  NodeMap node_map;
  auto key_map = GetRegistryKeyMap(registry);

  quintptr node_id_generator{};
  int module_level_row_generator{};
  int key_level_row_generator{};

  Node internal_root;
  Node::InternalRootData internal_root_data;

  for (auto it{key_map.begin()}; it != key_map.end(); ++it) {
    const auto &module_name = it.key();
    const auto &local_key_map = it.value();

    Node module_node;
    module_node.node_id = ++node_id_generator;
    module_node.parent_row = internal_root.row;
    module_node.row = module_level_row_generator++;

    internal_root_data.child_node_id_list.push_back(module_node.node_id);

    Node::ModuleRootData module_root_data{module_name, {}};

    key_level_row_generator = 0;

    for (auto it2{local_key_map.begin()}; it2 != local_key_map.end(); ++it2) {
      const auto &key_name = it2.key();
      const auto &key_info = it2.value();

      Node key_node;
      key_node.node_id = ++node_id_generator;
      key_node.parent_row = module_node.row;
      key_node.row = key_level_row_generator++;
      key_node.data_var = Node::ModuleKeyData{
          key_name, key_info.localized_name, key_info.description,
          key_info.type, module_node.node_id};

      module_root_data.child_node_id_list.push_back(key_node.node_id);

      node_map.insert({key_node.node_id, std::move(key_node)});
    }

    module_node.data_var = std::move(module_root_data);
    node_map.insert({module_node.node_id, std::move(module_node)});
  }

  internal_root.data_var = std::move(internal_root_data);
  node_map.insert({internal_root.node_id, std::move(internal_root)});

  return node_map;
}

}  // namespace

struct ConfigModel::PrivateData final {
  Registry &registry;
  NodeMap node_map;
  std::optional<Error> opt_last_error;

  PrivateData(Registry &registry_) : registry(registry_) {}
};

ConfigModel *ConfigModel::Create(Registry &registry, QObject *parent) {
  auto model = new ConfigModel(registry, parent);

  QAbstractItemModelTester model_tester(
      model, QAbstractItemModelTester::FailureReportingMode::Fatal);

  return model;
}

ConfigModel::~ConfigModel(void) {}

QModelIndex ConfigModel::index(int row, int column,
                               const QModelIndex &parent) const {
  if (!hasIndex(row, column, parent)) {
    return {};
  }

  quintptr parent_node_id{};
  if (parent.isValid()) {
    parent_node_id = parent.internalId();
  }

  auto node_map_it = d->node_map.find(parent_node_id);
  if (node_map_it == d->node_map.end()) {
    return {};
  }

  const auto &parent_node = node_map_it->second;

  std::vector<quintptr> child_node_id_list;
  if (std::holds_alternative<Node::InternalRootData>(parent_node.data_var)) {
    const auto &internal_root_data =
        std::get<Node::InternalRootData>(parent_node.data_var);

    child_node_id_list = internal_root_data.child_node_id_list;

  } else if (std::holds_alternative<Node::ModuleRootData>(
                 parent_node.data_var)) {
    const auto &module_root_data =
        std::get<Node::ModuleRootData>(parent_node.data_var);

    child_node_id_list = module_root_data.child_node_id_list;

  } else {
    return {};
  }

  auto child_node_id_list_it = std::next(child_node_id_list.begin(), row);
  if (child_node_id_list_it == child_node_id_list.end()) {
    return {};
  }

  const auto &child_node_id = *child_node_id_list_it;
  return createIndex(row, column, child_node_id);
}

QModelIndex ConfigModel::parent(const QModelIndex &index) const {
  if (!index.isValid()) {
    return {};
  }

  auto node_map_it = d->node_map.find(index.internalId());
  if (node_map_it == d->node_map.end()) {
    return {};
  }

  const auto &node = node_map_it->second;

  quintptr parent_node_id{};
  if (std::holds_alternative<Node::ModuleKeyData>(node.data_var)) {
    const auto &module_key_data = std::get<Node::ModuleKeyData>(node.data_var);

    parent_node_id = module_key_data.parent_node_id;

  } else {
    return QModelIndex();
  }

  node_map_it = d->node_map.find(parent_node_id);
  if (node_map_it == d->node_map.end()) {
    return {};
  }

  const auto &parent_node = node_map_it->second;
  return createIndex(parent_node.row, 0, parent_node.node_id);
}

int ConfigModel::rowCount(const QModelIndex &parent) const {
  if (parent.column() > 0) {
    return 0;
  }

  quintptr parent_node_id{};
  if (parent.isValid()) {
    parent_node_id = parent.internalId();
  }

  auto node_map_it = d->node_map.find(parent_node_id);
  if (node_map_it == d->node_map.end()) {
    return {};
  }

  const auto &node = node_map_it->second;

  if (std::holds_alternative<Node::InternalRootData>(node.data_var)) {
    const auto &internal_root_data =
        std::get<Node::InternalRootData>(node.data_var);

    return static_cast<int>(internal_root_data.child_node_id_list.size());

  } else if (std::holds_alternative<Node::ModuleRootData>(node.data_var)) {
    const auto &module_root_data =
        std::get<Node::ModuleRootData>(node.data_var);

    return static_cast<int>(module_root_data.child_node_id_list.size());

  } else {
    return 0;
  }
}

int ConfigModel::columnCount(const QModelIndex &) const {
  return d->node_map.empty() ? 0 : 2;
}

QVariant ConfigModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid()) {
    return {};
  }

  auto node_map_it = d->node_map.find(index.internalId());
  if (node_map_it == d->node_map.end()) {
    return {};
  }

  const auto &node = node_map_it->second;

  if (std::holds_alternative<Node::ModuleRootData>(node.data_var)) {
    if (index.column() != 0 || role != Qt::DisplayRole) {
      return {};
    }

    const auto &data = std::get<Node::ModuleRootData>(node.data_var);
    switch (role) {
      case Qt::DisplayRole: return data.name;
      default: return {};
    }

  } else if (std::holds_alternative<Node::ModuleKeyData>(node.data_var)) {
    const auto &data = std::get<Node::ModuleKeyData>(node.data_var);
    if (index.column() == 0 && role == Qt::DisplayRole) {
      return data.localized_key_name;

    } else if (index.column() != 1 ||
               (role != Qt::DisplayRole && role != Qt::ToolTipRole)) {
      return {};
    }

    QVariant value;

    switch (role) {
      case Qt::DisplayRole: {
        node_map_it = d->node_map.find(data.parent_node_id);
        if (node_map_it == d->node_map.end()) {
          throw std::logic_error("Invalid parent node id for module key node");
        }

        const auto &parent_node = node_map_it->second;
        if (!std::holds_alternative<Node::ModuleRootData>(
                parent_node.data_var)) {
          throw std::logic_error("Invalid parent for module key node");
        }

        const auto &module_root_data =
            std::get<Node::ModuleRootData>(parent_node.data_var);

        value = d->registry.Get(module_root_data.name, data.key_name);
        break;
      }

      case Qt::ToolTipRole: {
        value = data.description;
        break;
      }

      default: return {};
    }

    return value;

  } else {
    throw std::logic_error("Invalid internal state");
  }
}

Qt::ItemFlags ConfigModel::flags(const QModelIndex &index) const {
  if (!index.isValid() || index.column() != 1) {
    return Qt::NoItemFlags;
  }

  auto item_flags{QAbstractItemModel::flags(index)};

  auto node_id{index.internalId()};
  auto node_map_it = d->node_map.find(node_id);
  if (node_map_it == d->node_map.end()) {
    return item_flags;
  }

  const auto &node = node_map_it->second;

  if (std::holds_alternative<Node::ModuleKeyData>(node.data_var)) {
    item_flags |= Qt::ItemIsEditable;
  }

  return item_flags;
}

bool ConfigModel::setData(const QModelIndex &index, const QVariant &value,
                          int role) {
  if (!index.isValid() || index.column() != 1) {
    return false;
  }

  auto node_map_it = d->node_map.find(index.internalId());
  if (node_map_it == d->node_map.end()) {
    return false;
  }

  const auto &node = node_map_it->second;

  if (!std::holds_alternative<Node::ModuleKeyData>(node.data_var)) {
    return false;
  }

  const auto &model_key_data = std::get<Node::ModuleKeyData>(node.data_var);

  node_map_it = d->node_map.find(model_key_data.parent_node_id);
  if (node_map_it == d->node_map.end()) {
    return false;
  }

  const auto &parent_node = node_map_it->second;
  if (!std::holds_alternative<Node::ModuleRootData>(parent_node.data_var)) {
    return false;
  }

  const auto &model_root_data =
      std::get<Node::ModuleRootData>(parent_node.data_var);

  auto processed_value{value};
  switch (model_key_data.type) {
    case Registry::Type::Integer: {
      if (role != Qt::EditRole) {
        return false;
      }

      processed_value = value.toInt();
      break;
    }

    case Registry::Type::Boolean: {
      if (role != Qt::EditRole && role != Qt::CheckStateRole) {
        return false;
      }

      processed_value = value.toBool();
      break;
    }

    case Registry::Type::String:
    case Registry::Type::KeySequence:
    default: {
      if (role != Qt::EditRole) {
        return false;
      }

      break;
    }
  }

  auto res = d->registry.Set(model_root_data.name, model_key_data.key_name,
                             processed_value);

  if (!res.Succeeded()) {
    // This works exactly like QSqlTableModel; this error can be retrieved
    // with ConfigModel::LastError()
    d->opt_last_error =
        Error{model_root_data.name, model_key_data.key_name,
              model_key_data.localized_key_name, res.TakeError()};

    return false;

  } else {
    d->opt_last_error = std::nullopt;
    return true;
  }
}

std::optional<ConfigModel::Error> ConfigModel::LastError(void) const {
  return d->opt_last_error;
}

ConfigModel::ConfigModel(Registry &registry, QObject *parent)
    : QAbstractItemModel(parent),
      d(new PrivateData(registry)) {

  connect(&registry, &Registry::SchemaChanged, this,
          &ConfigModel::OnSchemaChange);

  OnSchemaChange();
}

void ConfigModel::OnSchemaChange(void) {
  emit beginResetModel();
  d->node_map = ImportRegistry(d->registry);
  emit endResetModel();
}

}  // namespace mx::gui
