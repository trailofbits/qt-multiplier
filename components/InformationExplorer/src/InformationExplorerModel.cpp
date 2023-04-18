/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "InformationExplorerModel.h"

#include <multiplier/ui/Util.h>
#include <multiplier/ui/Assert.h>

namespace mx::gui {

namespace {

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

void CreatePropertyHelper(
    InformationExplorerModel::Context &context,
    const InformationExplorerModel::Context::NodeID &parent_node_id,
    QStringList path_component_list,
    const std::unordered_map<int, QVariant> &value_map) {

  auto &current_node = context.node_map[parent_node_id];

  QString current_component = path_component_list.front();
  path_component_list.pop_front();

  if (path_component_list.isEmpty()) {
    auto child_id = InformationExplorerModel::GenerateNodeID(context);

    InformationExplorerModel::Context::Node property_node;
    property_node.parent_node_id = parent_node_id;
    property_node.data = InformationExplorerModel::Context::Node::PropertyData{
        current_component, value_map};

    current_node.child_id_list.push_back(child_id);
    context.node_map.insert({child_id, std::move(property_node)});

    return;
  }

  std::optional<InformationExplorerModel::Context::NodeID> opt_child_id;

  for (const auto &child_id : current_node.child_id_list) {
    const auto &child_node = context.node_map[child_id];

    if (!std::holds_alternative<
            InformationExplorerModel::Context::Node::SectionData>(
            child_node.data)) {
      continue;
    }

    const auto &section_data =
        std::get<InformationExplorerModel::Context::Node::SectionData>(
            child_node.data);

    if (section_data.name == current_component) {
      opt_child_id = child_id;
      break;
    }
  }

  if (!opt_child_id.has_value()) {
    opt_child_id = InformationExplorerModel::GenerateNodeID(context);
    const auto &child_id = opt_child_id.value();

    InformationExplorerModel::Context::Node section_node;
    section_node.parent_node_id = parent_node_id;
    section_node.data =
        InformationExplorerModel::Context::Node::SectionData{current_component};

    current_node.child_id_list.push_back(child_id);
    context.node_map.insert({child_id, std::move(section_node)});
  }

  CreatePropertyHelper(context, opt_child_id.value(), path_component_list,
                       value_map);
}

}  // namespace

struct InformationExplorerModel::PrivateData final {
  IDatabase::Ptr database;

  std::optional<RawEntityId> opt_active_entity_id;

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

  ResetContext(d->context);
  d->opt_active_entity_id = std::nullopt;

  VariantId vid = EntityId(entity_id).Unpack();
  if (!std::holds_alternative<DeclId>(vid) &&
      !std::holds_alternative<FileId>(vid) &&
      !(std::holds_alternative<MacroId>(vid) &&
        std::get<MacroId>(vid).kind == MacroKind::DEFINE_DIRECTIVE)) {

    CreateProperty(d->context, tr("Update Failed"));

  } else {
    CreateProperty(d->context, tr("Updating"));

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

  Context::NodeID parent_node_id{};
  if (parent.isValid()) {
    parent_node_id = parent.internalId();
  }

  auto node_map_it = d->context.node_map.find(parent_node_id);
  if (node_map_it == d->context.node_map.end()) {
    return QModelIndex();
  }

  const auto &node = node_map_it->second;
  const auto &node_id_list = node.child_id_list;

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
  if (!child.isValid()) {
    return QModelIndex();
  }

  auto child_node_id = child.internalId();

  auto node_map_it = d->context.node_map.find(child_node_id);
  if (node_map_it == d->context.node_map.end()) {
    return QModelIndex();
  }

  const auto &child_node = node_map_it->second;
  if (child_node.parent_node_id == 0) {
    return QModelIndex();
  }

  node_map_it = d->context.node_map.find(child_node.parent_node_id);
  if (node_map_it == d->context.node_map.end()) {
    return QModelIndex();
  }

  const auto &parent_node = node_map_it->second;

  auto it =
      std::find(parent_node.child_id_list.begin(),
                parent_node.child_id_list.end(), child_node.parent_node_id);

  auto row = std::distance(parent_node.child_id_list.begin(), it);

  return createIndex(static_cast<int>(row), 0, child_node.parent_node_id);
}

int InformationExplorerModel::rowCount(const QModelIndex &parent) const {
  return RowCount(d->context, parent);
}

int InformationExplorerModel::columnCount(const QModelIndex &) const {
  return 1;
}

QVariant InformationExplorerModel::data(const QModelIndex &index,
                                        int role) const {
  return Data(d->context, index, role);
}

void InformationExplorerModel::FutureResultStateChanged() {
  emit beginResetModel();
  ResetContext(d->context);

  if (d->future_result.isCanceled()) {
    CreateProperty(d->context, tr("Interrupted"));
    emit endResetModel();

    return;
  }

  auto future_result = d->future_result.takeResult();
  if (!future_result.Succeeded()) {
    CreateProperty(d->context, tr("Failed"));
    emit endResetModel();

    return;
  }

  auto entity_information = future_result.TakeValue();
  ImportEntityInformation(d->context, entity_information);

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

  ResetContext(d->context);
}

void InformationExplorerModel::CancelRunningRequest() {
  if (!d->future_result.isRunning()) {
    return;
  }

  d->future_result.cancel();
  d->future_result.waitForFinished();

  d->future_result = {};
}

void InformationExplorerModel::CreateProperty(
    Context &context, const QString &path,
    const std::unordered_map<int, QVariant> &value_map) {

  Assert(!path.isEmpty(), "The path should not be empty");

  CreatePropertyHelper(context, 0, path.split("|"), value_map);
}

quintptr InformationExplorerModel::GenerateNodeID(Context &context) {
  return ++context.node_id_generator;
}

void InformationExplorerModel::ResetContext(Context &context) {
  context = {};

  Context::Node root_node;
  root_node.data = Context::Node::SectionData{tr("ROOT")};
  context.node_map.insert({0, std::move(root_node)});
}

void InformationExplorerModel::ImportEntityInformation(
    Context &context, const EntityInformation &entity_information) {

  struct Filler final {
    QString base_path;
    const std::vector<EntityInformation::Selection> &source_container;
  };

  const std::vector<Filler> filler_list{
      {tr("Redeclarations") + "|", entity_information.redeclarations},
      {tr("Macros used") + "|", entity_information.macros_used},
      {tr("Callees") + "|", entity_information.callees},
      {tr("Callers") + "|", entity_information.callers},
      {tr("Assigned to") + "|", entity_information.assigned_tos},
      {tr("Assignements") + "|", entity_information.assignments},
      {tr("Includes") + "|", entity_information.includes},
      {tr("Included by") + "|", entity_information.include_bys},
      {tr("Top level entities") + "|", entity_information.top_level_entities},
  };

  struct Property final {
    QString display_role;
    RawEntityId entity_id{};
    std::optional<QString> opt_location_role;
  };

  std::unordered_map<QString, Property> property_list;

  auto L_recalculatePath = [](const QString &original_path,
                              const Property &property) -> QString {
    auto new_property_path = original_path + "|";
    if (property.opt_location_role.has_value()) {
      const auto &location_role = property.opt_location_role.value();
      new_property_path += location_role;
    } else {
      new_property_path += property.display_role;
    }

    return new_property_path;
  };

  for (const auto &filler : filler_list) {
    std::unordered_set<QString> grouped_names;

    for (const auto &selection : filler.source_container) {
      auto opt_name = NameOfEntity(selection.entity);

      std::optional<QString> opt_location;
      if (selection.location.has_value()) {
        const auto &location = selection.location.value();
        opt_location = GetFileName(location);
      }

      Property property;
      if (opt_name.has_value()) {
        property.display_role = opt_name.value();

      } else if (opt_location.has_value()) {
        property.display_role = opt_location.value();

      } else {
        continue;
      }

      property.opt_location_role = std::move(opt_location);
      property.entity_id = IdOfEntity(selection.entity);

      auto property_path = filler.base_path + property.display_role;
      if (property_list.count(property_path) == 1) {
        auto old_property = property_list[property_path];
        property_list.erase(property_path);

        auto new_property_path = L_recalculatePath(property_path, old_property);

        grouped_names.insert(property_path);
        property_list.insert(
            {std::move(new_property_path), std::move(old_property)});
      }

      if (grouped_names.count(property_path) == 1) {
        property_path = L_recalculatePath(property_path, property);
      }

      property_list.insert({std::move(property_path), std::move(property)});
    }
  }

  for (auto &property_list_p : property_list) {
    const auto &path = property_list_p.first;
    const auto &property = property_list_p.second;

    std::unordered_map<int, QVariant> value_map = {};
    value_map.insert(
        {IInformationExplorerModel::EntityIdRole, property.entity_id});

    if (property.opt_location_role.has_value()) {
      auto &location = property.opt_location_role.value();
      value_map.insert(
          {IInformationExplorerModel::LocationRole, std::move(location)});
    }

    CreateProperty(context, path, std::move(value_map));
  }

  context.entity_name = entity_information.name;

  auto variant_id = EntityId(entity_information.requested_id).Unpack();
  if (std::holds_alternative<FileId>(variant_id)) {
    std::filesystem::path path(context.entity_name.toStdString());
    context.entity_name = QString::fromStdString(path.filename());
  }
}

int InformationExplorerModel::RowCount(const Context &context,
                                       const QModelIndex &parent) {

  Context::NodeID node_id{};
  if (parent.isValid()) {
    node_id = parent.internalId();
  }

  auto node_map_it = context.node_map.find(node_id);
  if (node_map_it == context.node_map.end()) {
    return 0;
  }

  const auto &node = node_map_it->second;
  return static_cast<int>(node.child_id_list.size());
}

QVariant InformationExplorerModel::Data(const Context &context,
                                        const QModelIndex &index, int role) {

  if (!index.isValid()) {
    return QVariant();
  }

  auto node_id = index.internalId();

  auto node_map_it = context.node_map.find(node_id);
  if (node_map_it == context.node_map.end()) {
    return QVariant();
  }

  const auto &node = node_map_it->second;

  if (std::holds_alternative<
          InformationExplorerModel::Context::Node::SectionData>(node.data)) {

    const auto &section_data =
        std::get<InformationExplorerModel::Context::Node::SectionData>(
            node.data);

    if (role != Qt::DisplayRole) {
      return QVariant();
    }

    return section_data.name;

  } else {
    const auto &property_data =
        std::get<InformationExplorerModel::Context::Node::PropertyData>(
            node.data);

    if (role == Qt::DisplayRole) {
      return property_data.display_role;
    }

    auto value_map_it = property_data.value_map.find(role);
    if (value_map_it == property_data.value_map.end()) {
      return QVariant();
    }

    return value_map_it->second;
  }
}

}  // namespace mx::gui
