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
  if (opt_location.has_value()) {
    for (std::filesystem::path path : opt_location->file.paths()) {
      return QString("%1:%2:%3")
          .arg(QString::fromStdString(path.generic_string()))
          .arg(opt_location->line)
          .arg(opt_location->column);
    }
  }

  return std::nullopt;
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

    CreateStatusProperty(d->context, tr("Update Failed"));

  } else {
    CreateStatusProperty(d->context, tr("Updating"));

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

  if (0 > row || 0 > column) {
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

  const Context::Node &parent_node = node_map_it->second;
  if (!std::holds_alternative<Context::Node::SectionData>(parent_node.data)) {
    return QModelIndex();
  }

  const Context::Node::SectionData &parent_data =
      std::get<Context::Node::SectionData>(parent_node.data);

  const std::size_t node_index = static_cast<std::size_t>(row);
  const Context::NodeIDList &node_id_list = parent_data.child_id_list;
  if (node_index >= node_id_list.size()) {
    return QModelIndex();
  }

  Context::NodeID node_id = node_id_list[node_index];
  return createIndex(row, column, static_cast<quintptr>(node_id));
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

  const Context::Node &child_node = node_map_it->second;
  if (!child_node.id || !child_node.parent_node_id) {
    return QModelIndex();
  }

  Assert(child_node_id == child_node.id, "Inconsistent IDs.");

  node_map_it = d->context.node_map.find(child_node.parent_node_id);
  if (node_map_it == d->context.node_map.end()) {
    return QModelIndex();
  }

  const Context::Node &parent_node = node_map_it->second;
  if (!std::holds_alternative<Context::Node::SectionData>(parent_node.data)) {
    return QModelIndex();
  }

  const Context::Node::SectionData &parent_data =
      std::get<Context::Node::SectionData>(parent_node.data);

  auto it =
      std::find(parent_data.child_id_list.begin(),
                parent_data.child_id_list.end(), child_node_id);

  auto row = std::distance(parent_data.child_id_list.begin(), it);

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
    CreateStatusProperty(d->context, tr("Interrupted"));
    emit endResetModel();

    return;
  }

  auto future_result = d->future_result.takeResult();
  if (!future_result.Succeeded()) {
    CreateStatusProperty(d->context, tr("Failed"));
    emit endResetModel();

    return;
  }

  ImportEntityInformation(d->context, future_result.TakeValue());

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

void InformationExplorerModel::CreateStatusProperty(
    Context &context, const QString &path) {

  Assert(!path.isEmpty(), "The path should not be empty");

  Context::NodeID simple_id = GenerateNodeID(context);
  Context::Node &node = context.node_map[simple_id];
  Context::Node::PropertyData data;
  data.value_map.emplace(NameRole, path);
  data.value_map.emplace(LocationRole, path);
  node.data = std::move(data);
}

quintptr InformationExplorerModel::GenerateNodeID(Context &context) {
  return ++context.node_id_generator;
}

void InformationExplorerModel::ResetContext(Context &context) {
  context = {};

  Context::Node root_node;
  root_node.data = Context::Node::SectionData{tr("ROOT"), {}};
  context.node_map.insert({0, std::move(root_node)});
}

void InformationExplorerModel::ImportEntityInformation(
    Context &context, EntityInformation entity_information) {

  struct Category final {
    QString name;
    const std::vector<EntityInformation::Selection> &source_container;
  };

  const std::vector<Category> filler_list{
      {tr("Redeclarations"), entity_information.redeclarations},
      {tr("Macros used"), entity_information.macros_used},
      {tr("Callees"), entity_information.callees},
      {tr("Callers"), entity_information.callers},
      {tr("Assigned to"), entity_information.assigned_tos},
      {tr("Assignements"), entity_information.assignments},
      {tr("Includes"), entity_information.includes},
      {tr("Included by"), entity_information.include_bys},
      {tr("Top level entities"), entity_information.top_level_entities},
  };

  std::unordered_map<QString, Context::NodeID> path_to_node_id;

  auto L_get_parent_id = [&] (const QString &loc, const QString &name,
                              Context::NodeID sel_id,
                              Context::NodeID category_id) {

    const QString &path = name.isEmpty() ? loc : name;
    auto id_it = path_to_node_id.find(path);
    if (id_it == path_to_node_id.end()) {
      path_to_node_id.emplace(path, sel_id);
      return category_id;
    }

    Context::NodeID parent_or_prev_id = id_it->second;
    Context::Node &node = context.node_map[parent_or_prev_id];
    if (node.parent_node_id != category_id) {
      Assert(parent_or_prev_id == node.id, "Invalid node ID");
      Assert(std::holds_alternative<Context::Node::SectionData>(node.data),
             "Invalid section ID");
      return node.id;  // It's a group node.
    }

    // Need to make an intermediate node, change a child id of `node` into
    // this new group node, then reparent `node` into this new group node.
    Context::NodeID group_id = GenerateNodeID(context);
    Context::Node &group_node = context.node_map[group_id];
    Context::Node::SectionData data;
    group_node.id = group_id;
    group_node.parent_node_id = node.parent_node_id;

    data.name = path;
    group_node.data = std::move(data);

    // Reparent the old non-section node into the section.
    node.parent_node_id = group_id;

    // Make the next one look in the right spot.
    id_it->second = group_id;

    return group_id;
  };

  for (const Category &filler : filler_list) {

    Context::NodeID category_id = GenerateNodeID(context);
    Context::Node &category_node = context.node_map[category_id];
    Context::Node::SectionData section;
    section.name = filler.name;
    category_node.id = category_id;
    category_node.depth = 0;
    category_node.data = std::move(section);

    path_to_node_id.clear();

    for (const EntityInformation::Selection &sel :
             filler.source_container) {
      Context::NodeID sel_id = GenerateNodeID(context);
      Context::Node &sel_node = context.node_map[sel_id];
      Context::Node::PropertyData data;
      QString location;
      QString name;

      if (auto opt_name = NameOfEntity(sel.entity)) {
        name = std::move(opt_name.value());
        data.value_map.emplace(NameRole, name);
      }

      if(auto opt_location = GetFileName(sel.location)) {
        location = std::move(opt_location.value());
        data.value_map.emplace(LocationRole, location);
      }

      data.value_map.emplace(EntityIdRole, IdOfEntity(sel.entity));

      sel_node.id = sel_id;
      sel_node.parent_node_id = L_get_parent_id(location, name, sel_id,
                                                category_id);
      sel_node.data = std::move(data);
    }

    // Unlink this catrgory, we didn't add anything under it.
    if (path_to_node_id.empty()) {
      context.node_map.erase(category_id);
      continue;
    }
  }

  for (auto &entry : context.node_map) {
    Context::Node &node = entry.second;
    Assert(entry.first == node.id, "Invalid node id");
    if (node.id) {
      Context::Node &parent_node = context.node_map[node.parent_node_id];
      Context::Node::SectionData &parent_data =
          std::get<Context::Node::SectionData>(parent_node.data);
      parent_data.child_id_list.push_back(node.id);
      node.depth = parent_node.depth + 1;
    }
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

  const Context::Node &node = node_map_it->second;

  if (std::holds_alternative<Context::Node::PropertyData>(node.data)) {
    return 0;  // No children.

  } else if (std::holds_alternative<Context::Node::SectionData>(node.data)) {
    const Context::Node::SectionData &data =
        std::get<Context::Node::SectionData>(node.data);

    return static_cast<int>(data.child_id_list.size());

  } else {
    return 0;
  }
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

  const Context::Node &node = node_map_it->second;

  if (std::holds_alternative<Context::Node::SectionData>(node.data)) {

    const Context::Node::SectionData &section_data =
        std::get<Context::Node::SectionData>(node.data);

    if (role != Qt::DisplayRole) {
      return QVariant();
    }

    return section_data.name;

  } else if (std::holds_alternative<Context::Node::PropertyData>(node.data)) {
    const Context::Node::PropertyData &property_data =
        std::get<Context::Node::PropertyData>(node.data);

    // If we're at depth 1, then we haven't merged anything, so the display
    // role is the name, but otherwise, we've merged things, and so to
    // disambiguate the name, we return the location.
    if (role == Qt::DisplayRole) {
      QVariant name;
      if (auto it = property_data.value_map.find(NameRole);
          it != property_data.value_map.end()) {
        if (node.depth <= 2) {
          return it->second;
        }

        name.setValue(it->second);
      }

      if (auto it = property_data.value_map.find(LocationRole);
          it != property_data.value_map.end()) {
        return it->second;
      }

      return name;
    }

    auto value_map_it = property_data.value_map.find(role);
    if (value_map_it == property_data.value_map.end()) {
      return QVariant();
    }

    return value_map_it->second;
  }

  return QVariant();
}

}  // namespace mx::gui
