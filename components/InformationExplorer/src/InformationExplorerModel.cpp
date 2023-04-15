/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "InformationExplorerModel.h"

#include <multiplier/ui/Util.h>

#include <unordered_map>

namespace mx::gui {

namespace {

enum class ModelState {
  Ready,
  Updating,
  Failed,
  Interrupted,
};

const QString &GetModelStateName(const ModelState &model_state) {
  static const std::unordered_map<ModelState, QString> kLabelMap = {
      {ModelState::Ready, QObject::tr("Ready")},
      {ModelState::Updating, QObject::tr("Updating")},
      {ModelState::Failed, QObject::tr("Failed")},
      {ModelState::Interrupted, QObject::tr("Interrupted")},
  };

  auto it = kLabelMap.find(model_state);
  if (it == kLabelMap.end()) {
    static const QString kUnknownModelState{"Unknown"};
    return kUnknownModelState;
  }

  const auto &label = it->second;
  return label;
}

std::optional<QString>
GetFileName(const std::optional<EntityInformation::Location> &opt_location) {

  if (!opt_location.has_value()) {
    return std::nullopt;
  }

  const auto &location = opt_location.value();

  std::optional<QString> opt_name;
  for (std::filesystem::path path : location.file.paths()) {

    auto name = QString("%1:%2:%3")
                    .arg(QString::fromStdString(path.generic_string()))
                    .arg(location.line)
                    .arg(location.column);

    if (!name.isEmpty()) {
      opt_name = std::move(name);
    }

    break;
  }

  return opt_name;
}

}  // namespace

struct InformationExplorerModel::PrivateData final {
  IDatabase::Ptr database;

  std::optional<RawEntityId> opt_active_entity_id;

  ModelState model_state{ModelState::Ready};
  QFuture<IDatabase::EntityInformationResult> future_result;
  QFutureWatcher<IDatabase::EntityInformationResult> future_watcher;

  Context context;
};

void InformationExplorerModel::RequestEntityInformation(
    const RawEntityId &entity_id) {

  if (d->opt_active_entity_id.has_value() &&
      d->opt_active_entity_id.value() == entity_id) {
    return;
  }

  emit beginResetModel();
  CancelRunningRequest();

  d->context = {};
  d->opt_active_entity_id = std::nullopt;

  VariantId vid = EntityId(entity_id).Unpack();
  if (!std::holds_alternative<DeclId>(vid) &&
      !std::holds_alternative<FileId>(vid) &&
      !(std::holds_alternative<MacroId>(vid) &&
        std::get<MacroId>(vid).kind == MacroKind::DEFINE_DIRECTIVE)) {

    d->model_state = ModelState::Failed;

  } else {
    d->model_state = ModelState::Updating;
    d->opt_active_entity_id = entity_id;

    d->future_result = d->database->RequestEntityInformation(entity_id);
    d->future_watcher.setFuture(d->future_result);
  }

  emit endResetModel();
}

InformationExplorerModel::~InformationExplorerModel() {
  CancelRunningRequest();
}

QModelIndex InformationExplorerModel::index(int row, int column,
                                            const QModelIndex &parent) const {

  if (!hasIndex(row, column, parent)) {
    return QModelIndex();
  }

  if (d->model_state != ModelState::Ready) {
    if (!parent.isValid() && row == 0) {
      return createIndex(row, column);
    }

    return QModelIndex();
  }

  if (d->context.root_node_map.empty()) {
    return QModelIndex();
  }

  if (!parent.isValid()) {
    auto root_node_map_it = d->context.root_node_map.begin();
    root_node_map_it = std::next(root_node_map_it, row);

    if (root_node_map_it == d->context.root_node_map.end()) {
      return QModelIndex();
    }

    const auto &node_id = root_node_map_it->first;
    return createIndex(row, column, node_id);
  }

  auto parent_node_id = parent.internalId();

  auto root_node_map_it = d->context.root_node_map.find(parent_node_id);
  if (root_node_map_it == d->context.root_node_map.end()) {
    return QModelIndex();
  }

  const auto &node_id_list = root_node_map_it->second;
  if (static_cast<std::size_t>(row) >= node_id_list.size()) {
    return QModelIndex();
  }

  auto node_id_list_it = node_id_list.begin();
  node_id_list_it = std::next(node_id_list_it, row);

  if (node_id_list_it == node_id_list.end()) {
    return QModelIndex();
  }

  const auto &node_id = *node_id_list_it;
  return createIndex(row, column, node_id);
}

QModelIndex InformationExplorerModel::parent(const QModelIndex &child) const {
  if (d->model_state != ModelState::Ready) {
    return QModelIndex();
  }

  auto child_node_id = child.internalId();
  if (!child.isValid() || child_node_id < kTopLevelNodeIDMax) {
    return QModelIndex();
  }

  auto child_node_it = d->context.node_map.find(child_node_id);
  if (child_node_it == d->context.node_map.end()) {
    return QModelIndex();
  }

  const auto &child_node = child_node_it->second;

  auto it = d->context.root_node_map.find(child_node.parent_node);
  auto row = std::distance(d->context.root_node_map.begin(), it);
  return createIndex(static_cast<int>(row), 0, child_node.parent_node);
}

int InformationExplorerModel::rowCount(const QModelIndex &parent) const {
  if (d->model_state != ModelState::Ready) {
    return 1;
  }

  return RowCount(d->context, parent);
}

int InformationExplorerModel::columnCount(const QModelIndex &) const {
  return 1;
}

QVariant InformationExplorerModel::data(const QModelIndex &index,
                                        int role) const {
  if (d->model_state != ModelState::Ready) {
    auto parent = index.parent();
    if (!parent.isValid() && index.row() == 0 && index.column() == 0) {
      return GetModelStateName(d->model_state);
    }

    return QVariant();
  }

  return Data(d->context, index, role);
}

void InformationExplorerModel::FutureResultStateChanged() {
  emit beginResetModel();
  d->context = {};

  if (d->future_result.isCanceled()) {
    d->model_state = ModelState::Interrupted;
    emit endResetModel();

    return;
  }

  auto future_result = d->future_result.takeResult();
  if (!future_result.Succeeded()) {
    d->model_state = ModelState::Failed;
    emit endResetModel();

    return;
  }

  auto entity_information = future_result.TakeValue();
  ImportEntityInformation(d->context, entity_information);

  d->model_state = ModelState::Ready;
  emit endResetModel();
}

InformationExplorerModel::InformationExplorerModel(
    Index index, FileLocationCache file_location_cache, QObject *parent)
    : IInformationExplorerModel(parent),
      d(new PrivateData) {

  d->database = IDatabase::Create(index, file_location_cache);

  connect(&d->future_watcher,
          &QFutureWatcher<IDatabase::EntityInformationResult>::finished, this,
          &InformationExplorerModel::FutureResultStateChanged);
}

void InformationExplorerModel::CancelRunningRequest() {
  if (!d->future_result.isRunning()) {
    return;
  }

  d->future_result.cancel();
  d->future_result.waitForFinished();

  d->future_result = {};
}

quintptr InformationExplorerModel::GenerateNodeID(Context &context) {
  return ++context.node_id_generator;
}

void InformationExplorerModel::ImportEntityInformation(
    Context &context, const EntityInformation &entity_information) {

  struct Filler final {
    Context::NodeID destination;
    const std::vector<EntityInformation::Selection> &source_container;
  };

  const std::vector<Filler> filler_list{
      {kRedeclarationsNodeId, entity_information.redeclarations},
      {kMacrosUsedNodeId, entity_information.macros_used},
      {kCalleesNodeId, entity_information.callees},
      {kCallersNodeId, entity_information.callers},
      {kAssignedTo, entity_information.assigned_tos},
      {kAssignment, entity_information.assignments},
      {kIncludesNodeId, entity_information.includes},
      {kIncludeBysNodeId, entity_information.include_bys},
      {kTopLevelEntitiesNodeId, entity_information.top_level_entities},
  };

  for (const auto &filler : filler_list) {
    Context::NodeIDList node_list;

    for (const auto &selection : filler.source_container) {
      auto opt_name = NameOfEntity(selection.entity);
      if (!selection.location.has_value() && !opt_name.has_value()) {
        continue;
      }

      Context::Node node = {};
      node.parent_node = filler.destination;

      // The Context::Node::name field is the Qt::DisplayRole role. If
      // this is a file, then the node name is the location
      if (opt_name.has_value()) {
        node.name = std::move(opt_name.value());

      } else {
        const auto &location = selection.location.value();

        auto opt_file_name = GetFileName(location);
        if (!opt_file_name.has_value()) {
          continue;
        }

        node.name = std::move(opt_file_name.value());
      }

      auto node_id = GenerateNodeID(context);

      context.node_map.insert({node_id, std::move(node)});
      node_list.push_back(node_id);
    }

    if (!node_list.empty()) {
      context.root_node_map[filler.destination] = std::move(node_list);
    }
  }

  context.entity_name = entity_information.name;

  auto variant_id = EntityId(entity_information.requested_id).Unpack();
  if (std::holds_alternative<FileId>(variant_id)) {
    std::filesystem::path path(context.entity_name.toStdString());
    context.entity_name = QString::fromStdString(path.filename());
  }

  Context::NodeIDList node_list;
  auto L_addEntityInfoNode = [&](const QString &name) {
    auto node_id = GenerateNodeID(context);
    node_list.push_back(node_id);

    context.node_map.insert({node_id, {kEntityInformationNodeId, name}});
  };

  if (entity_information.entity.location.has_value()) {
    const auto &location = entity_information.entity.location.value();
    if (auto opt_file_name = GetFileName(location); opt_file_name.has_value()) {
      L_addEntityInfoNode(tr("Location: ") + opt_file_name.value());
    }
  }

  context.root_node_map[kEntityInformationNodeId] = std::move(node_list);
}

int InformationExplorerModel::RowCount(const Context &context,
                                       const QModelIndex &parent) {
  if (!parent.isValid()) {
    return static_cast<int>(context.root_node_map.size());
  }

  auto node_id = parent.internalId();
  if (node_id >= kTopLevelNodeIDMax) {
    return 0;
  }

  auto node_list_it = context.root_node_map.find(node_id);
  if (node_list_it == context.root_node_map.end()) {
    return 0;
  }

  const auto &node_list = node_list_it->second;
  return static_cast<int>(node_list.size());
}

QVariant InformationExplorerModel::Data(const Context &context,
                                        const QModelIndex &index, int role) {

  if (role != Qt::DisplayRole) {
    return QVariant();
  }

  auto node_id = index.internalId();
  if (node_id < kTopLevelNodeIDMax) {
    switch (node_id) {
      case kEntityInformationNodeId: {
        return context.entity_name;
      }

      case kRedeclarationsNodeId: return tr("Redeclarations");
      case kMacrosUsedNodeId: return tr("Macros used");
      case kCalleesNodeId: return tr("Callees");
      case kCallersNodeId: return tr("Callers");
      case kAssignedTo: return tr("Assigned to");
      case kAssignment: return tr("Assignment");
      case kIncludesNodeId: return tr("Includes");
      case kIncludeBysNodeId: return tr("Included by");
      case kTopLevelEntitiesNodeId: return tr("Top level entities");
      default: break;
    }
  }

  auto node_it = context.node_map.find(node_id);
  if (node_it == context.node_map.end()) {
    return QVariant();
  }

  const auto &node = node_it->second;
  return node.name;
}

}  // namespace mx::gui
