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
#include <QTimer>

namespace mx::gui {

namespace {

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
    InformationExplorerModel::NodeIDPath &generated_node_id_path,
    InformationExplorerModel::NodeIDPath current_node_id_path, QStringList path,
    const std::unordered_map<int, QVariant> &value_map) {

  auto parent_node_id = current_node_id_path.back();
  auto &current_node = context.node_map[parent_node_id];

  QString current_component = path.front();
  path.pop_front();

  if (path.isEmpty()) {
    generated_node_id_path = std::move(current_node_id_path);

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

    current_node_id_path.push_back(child_id);

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

  current_node_id_path.push_back(opt_child_id.value());
  CreatePropertyHelper(context, generated_node_id_path, current_node_id_path,
                       path, value_map);
}

}  // namespace

struct InformationExplorerModel::PrivateData final {
  Index index;
  FileLocationCache file_location_cache;

  std::optional<RawEntityId> opt_active_entity_id;

  IDatabase::Ptr database;
  QFuture<bool> request_status_future;
  QFutureWatcher<bool> future_watcher;

  QTimer import_timer;
  std::mutex data_batch_mutex;
  std::vector<DataBatch> data_batch_queue;

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

  CancelRunningRequest();

  emit beginResetModel();
  ResetContext(d->context);
  emit endResetModel();

  d->opt_active_entity_id = std::nullopt;
  d->request_status_future =
      d->database->RequestEntityInformation(*this, entity_id);

  d->future_watcher.setFuture(d->request_status_future);
  d->import_timer.start(250);
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
  if (d->request_status_future.isCanceled()) {
    emit beginResetModel();
    d->opt_active_entity_id = std::nullopt;
    ResetContext(d->context);
    emit endResetModel();

    return;
  }

  auto request_status = d->request_status_future.takeResult();
  if (!request_status) {
    emit beginResetModel();
    d->opt_active_entity_id = std::nullopt;
    ResetContext(d->context);
    emit endResetModel();

    return;
  }
}

InformationExplorerModel::InformationExplorerModel(
    Index index, FileLocationCache file_location_cache, QObject *parent)
    : IInformationExplorerModel(parent),
      d(new PrivateData) {

  d->database = IDatabase::Create(index, file_location_cache);

  d->index = index;
  d->file_location_cache = file_location_cache;

  connect(&d->future_watcher, &QFutureWatcher<QFuture<bool>>::finished, this,
          &InformationExplorerModel::FutureResultStateChanged);

  connect(&d->import_timer, &QTimer::timeout, this,
          &InformationExplorerModel::ProcessDataBatchQueue);

  ResetContext(d->context);
}

void InformationExplorerModel::CancelRunningRequest() {
  d->import_timer.stop();

  if (d->request_status_future.isRunning()) {
    d->request_status_future.cancel();
    d->request_status_future.waitForFinished();
    d->request_status_future = {};
  }
}

void InformationExplorerModel::OnDataBatch(DataBatch data_batch) {
  std::lock_guard<std::mutex> lock(d->data_batch_mutex);
  d->data_batch_queue.push_back(std::move(data_batch));
}

void InformationExplorerModel::CreateProperty(
    Context &context, NodeIDPath &node_id_path, const QStringList &path,
    const std::unordered_map<int, QVariant> &value_map) {

  Assert(!path.isEmpty(), "The path should not be empty");

  CreatePropertyHelper(context, node_id_path, {0}, path, value_map);
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
    Context &context, NodeIDPathList &generated_node_id_path_list,
    const EntityInformation &entity_information) {

  struct Filler final {
    QStringList base_path;
    const std::vector<EntityInformation::Selection> &source_container;
  };

  const Filler filler_list[] = {
      {{tr("Definitions")}, entity_information.definitions},
      {{tr("Declarations")}, entity_information.declarations},
      {{tr("Enumerators")}, entity_information.enumerators},
      {{tr("Member variables")}, entity_information.members},
      {{tr("Parameters")}, entity_information.parameters},
      {{tr("Variables")}, entity_information.variables},
      {{tr("Callees")}, entity_information.callees},
      {{tr("Callers")}, entity_information.callers},
      {{tr("Pointers")}, entity_information.pointers},
      {{tr("Assigned to")}, entity_information.assigned_tos},
      {{tr("Assignments")}, entity_information.assignments},
      {{tr("Address ofs")}, entity_information.address_ofs},
      {{tr("Dereferences")}, entity_information.dereferences},
      {{tr("Call arguments")}, entity_information.arguments},
      {{tr("Conditional tests")}, entity_information.tests},
      {{tr("Casts")}, entity_information.type_casts},
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

  for (const Filler &filler : filler_list) {
    std::unordered_map<QString, Property> property_map = {};

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

      NodeIDPath node_id_path;
      CreateProperty(context, node_id_path, property.path, property.value_map);

      generated_node_id_path_list.push_back(std::move(node_id_path));
    }
  }

  auto variant_id = EntityId(entity_information.requested_id).Unpack();
  if (std::holds_alternative<FileId>(variant_id)) {
    std::filesystem::path path(context.entity_name.toStdString());
    context.entity_name = QString::fromStdString(path.filename());

  } else {
    context.entity_name = entity_information.name;
  }
}

void InformationExplorerModel::ProcessDataBatchQueue() {
  if (!d->request_status_future.isRunning()) {
    d->import_timer.stop();
  }

  std::vector<DataBatch> data_batch_queue;

  {
    std::lock_guard<std::mutex> lock(d->data_batch_mutex);

    data_batch_queue = std::move(d->data_batch_queue);
    d->data_batch_queue.clear();
  }

  if (data_batch_queue.empty()) {
    return;
  }

  NodeIDPathList node_id_path_list;
  auto old_context = d->context;

  for (const auto &data_batch : data_batch_queue) {
    for (const auto &entity_information : data_batch) {
      if (!d->opt_active_entity_id.has_value() ||
          d->opt_active_entity_id.value() != entity_information.requested_id) {

        d->opt_active_entity_id = entity_information.requested_id;
      }

      NodeIDPathList new_node_id_path_list;
      ImportEntityInformation(d->context, new_node_id_path_list,
                              entity_information);

      node_id_path_list.insert(
          node_id_path_list.end(),
          std::make_move_iterator(new_node_id_path_list.begin()),
          std::make_move_iterator(new_node_id_path_list.end()));
    }
  }

  std::unordered_set<Context::NodeID> emitted_node_list;

  for (const auto &node_id_path : node_id_path_list) {
    const auto &parent_node_id = node_id_path[1];

    const auto &root_node = d->context.node_map[0];
    auto child_id_list_it =
        std::find(root_node.child_id_list.begin(),
                  root_node.child_id_list.end(), parent_node_id);

    auto parent_row_number = static_cast<int>(
        std::distance(root_node.child_id_list.begin(), child_id_list_it));

    if (old_context.node_map.count(parent_node_id) == 0) {
      if (emitted_node_list.count(parent_node_id) != 0) {
        continue;
      }

      emitted_node_list.insert(parent_node_id);

      emit beginInsertRows(QModelIndex(), parent_row_number, parent_row_number);
      emit endInsertRows();

      continue;
    }

    const auto &child_node_id = node_id_path[2];

    if (old_context.node_map.count(child_node_id) == 0) {
      if (emitted_node_list.count(child_node_id) != 0) {
        continue;
      }

      emitted_node_list.insert(child_node_id);

      const auto &parent_node = d->context.node_map[parent_node_id];

      child_id_list_it =
          std::find(parent_node.child_id_list.begin(),
                    parent_node.child_id_list.end(), child_node_id);

      auto child_row_number = static_cast<int>(
          std::distance(parent_node.child_id_list.begin(), child_id_list_it));

      auto parent_index = index(parent_row_number, 0, QModelIndex());

      emit beginInsertRows(parent_index, child_row_number, child_row_number);
      emit endInsertRows();
    }
  }
}

}  // namespace mx::gui
