/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "InformationExplorerModel.h"

#include <multiplier/ui/Util.h>
#include <multiplier/ui/Assert.h>

#include <QFutureWatcher>

namespace mx::gui {

namespace {

QString PathToString(const QStringList &path) {
  return path.join(".");
}

static std::optional<QString> GetName(const QVariant &display_role) {
  if (!display_role.isValid()) {
    return std::nullopt;
  }

  std::string_view data;

  if (display_role.canConvert<QString>()) {
    return qvariant_cast<QString>(display_role);
  } else if (display_role.canConvert<TokenRange>()) {
    data = qvariant_cast<TokenRange>(display_role).data();
  } else if (display_role.canConvert<Token>()) {
    data = qvariant_cast<Token>(display_role).data();
  } else {
    return std::nullopt;
  }

  if (data.empty()) {
    return std::nullopt;
  }

  return QString::fromUtf8(data.data(), static_cast<qsizetype>(data.size()));
}

std::optional<QString>
GetFileName(const std::optional<EntityInformation::Location> &opt_location,
            bool path_only) {

  if (!opt_location.has_value()) {
    return std::nullopt;
  }

  const auto &location = opt_location.value();

  std::optional<QString> opt_name;
  for (std::filesystem::path path : location.file.paths()) {
    if (path_only) {
      return QString::fromStdString(path.generic_string());
    }

    return QString("%1:%2:%3")
        .arg(QString::fromStdString(path.generic_string()))
        .arg(location.line)
        .arg(location.column);
  }

  return std::nullopt;
}

void CreatePropertyHelper(
    InformationExplorerModel::Context &context,
    const InformationExplorerModel::Context::NodeID &parent_node_id,
    QStringList path, const std::unordered_map<int, QVariant> &value_map) {

  auto &current_node = context.node_map[parent_node_id];

  QString current_component = path.front();
  path.pop_front();

  if (path.isEmpty()) {
    for (const auto &child_id : current_node.child_id_list) {
      auto &child_node = context.node_map[child_id];

      if (child_node.display_role == current_component) {
        child_node.value_map = std::move(value_map);
        return;
      }
    }

    auto child_id = InformationExplorerModel::GenerateNodeID(context);

    InformationExplorerModel::Context::Node property_node;
    property_node.parent_node_id = parent_node_id;
    property_node.display_role = current_component;
    property_node.value_map = value_map;

    current_node.child_id_list.push_back(child_id);
    context.node_map.insert({child_id, std::move(property_node)});

    return;
  }

  std::optional<InformationExplorerModel::Context::NodeID> opt_child_id;

  for (const auto &child_id : current_node.child_id_list) {
    const auto &child_node = context.node_map[child_id];

    if (child_node.display_role == current_component) {
      opt_child_id = child_id;
      break;
    }
  }

  if (!opt_child_id.has_value()) {
    opt_child_id = InformationExplorerModel::GenerateNodeID(context);
    const auto &child_id = opt_child_id.value();

    InformationExplorerModel::Context::Node section_node;
    section_node.parent_node_id = parent_node_id;
    section_node.display_role = current_component;

    current_node.child_id_list.push_back(child_id);
    context.node_map.insert({child_id, std::move(section_node)});
  }

  CreatePropertyHelper(context, opt_child_id.value(), path, value_map);
}

}  // namespace

struct InformationExplorerModel::PrivateData final {
  IDatabase::Ptr database;

  Index index;
  FileLocationCache file_location_cache;

  std::optional<RawEntityId> opt_active_entity_id;

  QFuture<IDatabase::EntityInformationResult> future_result;
  QFutureWatcher<IDatabase::EntityInformationResult> future_watcher;

  Context context;
};

Index InformationExplorerModel::GetIndex() const {
  return d->index;
}

FileLocationCache InformationExplorerModel::GetFileLocationCache() const {
  return d->file_location_cache;
}

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

  CreateProperty(d->context, {tr("Updating...")});

  d->future_result = d->database->RequestEntityInformation(entity_id);
  d->future_watcher.setFuture(d->future_result);

  emit endResetModel();
}

std::optional<RawEntityId>
InformationExplorerModel::GetCurrentEntityID() const {
  return d->opt_active_entity_id;
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

  node_map_it = d->context.node_map.find(parent_node.parent_node_id);
  if (node_map_it == d->context.node_map.end()) {
    return QModelIndex();
  }

  const auto &grandparent_node = node_map_it->second;

  auto it = std::find(grandparent_node.child_id_list.begin(),
                      grandparent_node.child_id_list.end(),
                      child_node.parent_node_id);

  auto row = std::distance(grandparent_node.child_id_list.begin(), it);

  return createIndex(static_cast<int>(row), 0, child_node.parent_node_id);
}

int InformationExplorerModel::rowCount(const QModelIndex &parent) const {
  Context::NodeID node_id{};
  if (parent.isValid()) {
    node_id = parent.internalId();
  }

  auto node_map_it = d->context.node_map.find(node_id);
  if (node_map_it == d->context.node_map.end()) {
    return 0;
  }

  const auto &node = node_map_it->second;
  return static_cast<int>(node.child_id_list.size());
}

int InformationExplorerModel::columnCount(const QModelIndex &) const {
  return 1;
}

QVariant InformationExplorerModel::data(const QModelIndex &index,
                                        int role) const {
  if (!index.isValid()) {
    return QVariant();
  }

  auto node_id = index.internalId();

  auto node_map_it = d->context.node_map.find(node_id);
  if (node_map_it == d->context.node_map.end()) {
    return QVariant();
  }

  const auto &node = node_map_it->second;
  if (role == Qt::DisplayRole) {
    return node.display_role;
  }

  auto value_map_it = node.value_map.find(role);
  if (value_map_it == node.value_map.end()) {
    return QVariant();
  }

  return value_map_it->second;
}

void InformationExplorerModel::FutureResultStateChanged() {
  emit beginResetModel();
  ResetContext(d->context);

  if (d->future_result.isCanceled()) {
    CreateProperty(d->context, {tr("Interrupted")});
    emit endResetModel();
    return;
  }

  auto future_result = d->future_result.takeResult();
  if (!future_result.Succeeded()) {
    CreateProperty(d->context, {tr("Failed")});
    emit endResetModel();
    return;
  }

  auto entity_information = future_result.TakeValue();
  ImportEntityInformation(d->context, entity_information);

  if (d->context.node_map.size() > 1) {
    d->opt_active_entity_id = entity_information.requested_id;
  }

  emit endResetModel();
}

InformationExplorerModel::InformationExplorerModel(
    Index index, FileLocationCache file_location_cache, QObject *parent)
    : IInformationExplorerModel(parent),
      d(new PrivateData) {

  d->database = IDatabase::Create(index, file_location_cache);

  d->index = index;
  d->file_location_cache = file_location_cache;

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
    Context &context, const QStringList &path,
    const std::unordered_map<int, QVariant> &value_map) {

  Assert(!path.isEmpty(), "The path should not be empty");

  CreatePropertyHelper(context, 0, path, value_map);
}

quintptr InformationExplorerModel::GenerateNodeID(Context &context) {
  return ++context.node_id_generator;
}

void InformationExplorerModel::ResetContext(Context &context) {
  context = {};

  Context::Node root_node;
  root_node.display_role = tr("ROOT");
  context.node_map.insert({0, std::move(root_node)});
}

void InformationExplorerModel::ImportEntityInformation(
    Context &context, const EntityInformation &entity_information) {

  struct Filler final {
    QStringList base_path;
    const std::vector<EntityInformation::Selection> &source_container;
  };

  const Filler filler_list[] = {
      {{tr("Definitions")}, entity_information.definitions},
      {{tr("Declarations")}, entity_information.declarations},
      {{tr("Callees")}, entity_information.callees},
      {{tr("Callers")}, entity_information.callers},
      {{tr("Pointers")}, entity_information.pointers},
      {{tr("Assigned to")}, entity_information.assigned_tos},
      {{tr("Assignments")}, entity_information.assignments},
      {{tr("Address ofs")}, entity_information.address_ofs},
      {{tr("Dereferences")}, entity_information.dereferences},
      {{tr("Call arguments")}, entity_information.arguments},
      {{tr("Conditional tests")}, entity_information.tests},
      {{tr("Uses")}, entity_information.uses},
      {{tr("Includes")}, entity_information.includes},
      {{tr("Included by")}, entity_information.include_bys},
      {{tr("Macros used")}, entity_information.macros_used},
      {{tr("Top level entities")}, entity_information.top_level_entities},
  };

  struct Property final {
    QStringList path;
    QString display_role;
    std::unordered_map<int, QVariant> value_map;
  };

  std::vector<Property> property_list;

  auto L_recalculatePath =
      [](const std::unordered_map<QString, Property> &property_map,
         Property &property) {
        const auto &entity_id_role_var =
            property.value_map[IInformationExplorerModel::EntityIdRole];

        auto entity_id = qvariant_cast<RawEntityId>(entity_id_role_var);

        // If we are here, the display name is already the same. Attempt to add
        // the location role to make it easier to read
        if (property.value_map.count(IInformationExplorerModel::LocationRole) ==
            1) {
          const auto &location_role_var =
              property.value_map[IInformationExplorerModel::LocationRole];

          property.path.append(location_role_var.toString());

          auto key = PathToString(property.path);
          if (property_map.count(key) > 0) {
            auto suffix = QString(" #") + QString::number(entity_id);
            property.path.back().append(suffix);
          }

        } else {
          property.path.back().append(QString::number(entity_id));
        }
      };

  for (const Filler &filler : filler_list) {
    // Items will only be re-parented once at the first display
    // name conflict and only in the first level.
    // Additional entries that can't be made unique
    // by recalculating the path will be discarded.
    std::unordered_map<QString, Property> property_map = {};
    std::unordered_map<QString, bool> visited_name_map = {};

    for (const EntityInformation::Selection &selection :
         filler.source_container) {

      std::optional<QString> opt_name = GetName(selection.display_role);
      std::optional<QString> opt_location =
          GetFileName(selection.location, false);

      Property property;
      if (opt_name.has_value()) {
        property.display_role = opt_name.value();

      } else if (opt_location.has_value()) {
        property.display_role = opt_location.value();

      } else {
        continue;
      }

      property.path = filler.base_path;
      property.path.append(property.display_role);

      auto entity_id = static_cast<quint64>(IdOfEntity(selection.entity_role));
      property.value_map.emplace(IInformationExplorerModel::EntityIdRole,
                                 entity_id);

      if (selection.display_role.canConvert<TokenRange>()) {
        property.value_map.emplace(IInformationExplorerModel::TokenRangeRole,
                                   selection.display_role);

      } else if (selection.display_role.canConvert<Token>()) {
        TokenRange range(qvariant_cast<Token>(selection.display_role));
        QVariant res;
        res.setValue(std::move(range));
        property.value_map.emplace(IInformationExplorerModel::TokenRangeRole,
                                   std::move(res));
      }

      if (opt_location.has_value()) {
        property.value_map.emplace(IInformationExplorerModel::LocationRole,
                                   std::move(opt_location.value()));


        RawLocation raw_location;
        raw_location.path = GetFileName(selection.location, true).value();
        raw_location.line_number = selection.location->line;
        raw_location.column_number = selection.location->column;

        QVariant value;
        value.setValue(raw_location);

        property.value_map.insert(
            {InformationExplorerModel::RawLocationRole, std::move(value)});
      }

      QString property_key = PathToString(property.path);

      auto visited_name_map_it = visited_name_map.find(property_key);
      if (visited_name_map_it != visited_name_map.end()) {
        auto &fix_previous_entry = visited_name_map_it->second;

        if (fix_previous_entry) {
          // Keep the old entry as is; it will become the root of our duplicated
          // entities.
          //
          // Then, duplicate this property with a different path and turn off
          // token painting for it.
          auto &root_property = property_map[property_key];
          root_property.value_map.insert(
              {InformationExplorerModel::AutoExpandRole, false});

          auto old_property = root_property;
          old_property.value_map.insert(
              {InformationExplorerModel::ForceTextPaintRole, true});

          L_recalculatePath(property_map, old_property);
          fix_previous_entry = false;

          property_map.emplace(PathToString(old_property.path), old_property);
        }

        // Disable token painting for this item. Since the name was duplicated,
        // we either have a file location or (if one is not available) an
        // entity id number.
        property.value_map.insert(
            {InformationExplorerModel::ForceTextPaintRole, true});

        L_recalculatePath(property_map, property);
        property_key = PathToString(property.path);

      } else {
        visited_name_map.insert({property_key, true});
      }

      property_map.insert({std::move(property_key), std::move(property)});
    }

    for (auto &property_map_p : property_map) {
      auto &property = property_map_p.second;

      auto &value_map = property.value_map;
      value_map.insert({Qt::DisplayRole, property.display_role});
      property.display_role.clear();

      property_list.push_back(std::move(property));
    }
  }

  for (auto &property : property_list) {
    auto &value_map = property.value_map;
    CreateProperty(context, property.path, std::move(value_map));
  }

  context.entity_name = entity_information.name;

  auto variant_id = EntityId(entity_information.requested_id).Unpack();
  if (std::holds_alternative<FileId>(variant_id)) {
    std::filesystem::path path(context.entity_name.toStdString());
    context.entity_name = QString::fromStdString(path.filename());
  }
}

}  // namespace mx::gui
