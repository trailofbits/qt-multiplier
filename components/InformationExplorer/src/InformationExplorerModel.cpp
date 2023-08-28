/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "InformationExplorerModel.h"

#include <multiplier/ui/Util.h>
#include <multiplier/ui/Assert.h>

#include <QDebug>
#include <QFutureWatcher>
#include <QMap>
#include <QString>
#include <QTimer>
#include <QVariant>

namespace mx::gui {
namespace {

static QString GetName(const QVariant &display_role) {
  if (!display_role.isValid()) {
    return QString();
  }

  std::string_view data;

  if (display_role.canConvert<QString>()) {
    return qvariant_cast<QString>(display_role);
  } else if (display_role.canConvert<TokenRange>()) {
    data = qvariant_cast<TokenRange>(display_role).data();
  } else if (display_role.canConvert<Token>()) {
    data = qvariant_cast<Token>(display_role).data();
  } else {
    return QString();
  }

  if (data.empty()) {
    return QString();
  }

  return QString::fromUtf8(data.data(), static_cast<qsizetype>(data.size()));
}

static TokenRange GetTokenRange(const QVariant &display) {
  if (display.canConvert<TokenRange>()) {
    return qvariant_cast<TokenRange>(display);

  } else if (display.canConvert<Token>()) {
    return qvariant_cast<Token>(display);

  } else {
    return TokenRange();
  }
}

static QString GetFileName(const std::optional<EntityLocation> &opt_location,
                           bool path_only) {

  if (!opt_location.has_value()) {
    return QString();
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

  return QString();
}

static QString GetFileName(
    const std::optional<InformationExplorerModel::RawLocation> &opt_location,
    bool path_only) {

  if (!opt_location.has_value()) {
    return QString();
  }

  if (path_only) {
    return opt_location->path;
  }

  return QString("%1:%2:%3")
      .arg(opt_location->path)
      .arg(opt_location->line_number)
      .arg(opt_location->column_number);
}

static std::optional<InformationExplorerModel::RawLocation>
ConvertLocation(const std::optional<EntityLocation> &opt_location) {
  if (!opt_location) {
    return std::nullopt;
  }

  InformationExplorerModel::RawLocation location;
  location.column_number = opt_location->column;
  location.line_number = opt_location->line;
  for (std::filesystem::path path : opt_location->file.paths()) {
    location.path = QString::fromStdString(path.generic_string());
    return location;
  }

  return std::nullopt;
}

struct Node;

struct EntityData {
  VariantEntity entity;
  std::optional<InformationExplorerModel::RawLocation> location;
};

struct LocationData {
  std::vector<Node *> children;
};

struct CategoryData {

  //! The key for this node. It generally corresponds to the string form of
  //! `display`. Nodes are deduplicated by `key`.
  QString key;

  std::vector<Node *> ordered_children;
  QMap<QString, Node *> keyed_children;
};

struct RootData {
  std::vector<Node *> children;
};

struct Node {
  //! Link to this node's parent. If `nullptr`, then this is a root node.
  Node *parent{nullptr};

  //! The version number when this node was created.
  uint64_t version{0u};

  //! The row number of this node within its parent.
  int row{0};

  //! The number of children in this node.
  int child_count{0};

  //! What gets displayed for this node. This could be a QString, a Token, or
  //! a TokenRange.
  QString display;
  TokenRange token_range;

  //! The data of this node.
  std::variant<std::monostate, RootData, CategoryData, LocationData, EntityData>
      data;
};

struct Change {
  Node *parent{nullptr};
  int num_children_added{0};
};

}  // namespace

struct InformationExplorerModel::PrivateData final {
  Index index;
  FileLocationCache file_location_cache;

  RawEntityId next_active_entity_id{kInvalidEntityId};
  RawEntityId active_entity_id{kInvalidEntityId};
  std::optional<QString> opt_active_entity_name;

  IDatabase::Ptr database;
  QFuture<bool> info_request_status_future;
  QFutureWatcher<bool> info_future_watcher;

  QFuture<Token> name_request_future;
  QFutureWatcher<Token> name_future_watcher;

  // The QFuture that goes and loads entity information can sometimes send
  // *a lot* of data, so it sends it in batches, via invoking `OnDataBatch`.
  // When we get a batch of data, we add it to `data_batch_queue`. We use a
  // timer to periodically process the collected data batches. If we processed
  // them as they came, then we might get so many that we starve the UI thread.
  QTimer import_timer;
  std::mutex data_batch_mutex;
  std::vector<DataBatch> data_batch_queue;

  // All nodes in our tree has a version number, which is derived from this
  // value, which increments over time. If we're adding a node, and its version
  // number exceeds its parent's version number, then we want to trigger an
  // event for just that row, otherwise we want to trigger an event for the
  // parent of that row.
  uint64_t version{0u};

  std::deque<Change> change_list;
  std::unordered_map<Node *, Change *> changes;

  //! Model data
  std::deque<Node> nodes;
  Node *root{nullptr};

  void OnNewNode(InformationExplorerModel &self, Node *node);
  void OnNodeChanged(InformationExplorerModel &self, Node *node);
};

void InformationExplorerModel::PrivateData::OnNodeChanged(
    InformationExplorerModel &self, Node *node) {

  // If the node version is the current version, then we don't need to signal
  // a change because it has never been rendered.
  if (node->version != version) {
    QModelIndex node_index = self.createIndex(node->row, 0, node);
    emit self.dataChanged(node_index, node_index);
  }
}

void InformationExplorerModel::PrivateData::OnNewNode(
    InformationExplorerModel &, Node *node) {

  // If our node's parent is also new, then increment the node's parent's
  // child count, rather than deferring that increment to a `Change`, because
  // we know that there will be a corresponding `Change` for the node's
  // grand-parent.
  if (node->version == node->parent->version) {
    node->parent->child_count++;
    return;
  }

  Change *&change = changes[node->parent];
  if (!change) {
    change = &(change_list.emplace_back());
    change->parent = node->parent;
  }

  change->num_children_added++;
}

void InformationExplorerModel::ClearTree(void) {
  d->nodes.clear();
  d->root = &(d->nodes.emplace_back());
  d->root->data = RootData{};
  d->active_entity_id = kInvalidEntityId;
  d->next_active_entity_id = kInvalidEntityId;
  d->change_list.clear();
  d->changes.clear();
}

Index InformationExplorerModel::GetIndex() const {
  return d->index;
}

FileLocationCache InformationExplorerModel::GetFileLocationCache() const {
  return d->file_location_cache;
}

void InformationExplorerModel::RequestEntityInformation(
    const RawEntityId &entity_id) {

  if (d->active_entity_id == entity_id ||
      d->next_active_entity_id == entity_id) {
    return;
  }

  CancelRunningRequest();

  emit beginResetModel();
  ClearTree();
  emit endResetModel();

  d->active_entity_id = kInvalidEntityId;
  d->opt_active_entity_name = std::nullopt;
  d->next_active_entity_id = entity_id;
  d->info_request_status_future =
      d->database->RequestEntityInformation(*this, entity_id);

  d->info_future_watcher.setFuture(d->info_request_status_future);
  d->import_timer.start(250);

  d->name_request_future = d->database->RequestEntityName(entity_id);

  d->name_future_watcher.setFuture(d->name_request_future);
}

RawEntityId InformationExplorerModel::GetCurrentEntityID(void) const {
  return d->active_entity_id;
}

std::optional<QString> InformationExplorerModel::GetCurrentEntityName() const {
  return d->opt_active_entity_name;
}

InformationExplorerModel::~InformationExplorerModel(void) {
  CancelRunningRequest();
}

QModelIndex InformationExplorerModel::index(int row, int column,
                                            const QModelIndex &parent) const {

  if (!hasIndex(row, column, parent)) {
    return QModelIndex();
  }

  if (column != 0) {
    return QModelIndex();
  }

  Node *node = d->root;
  if (parent.isValid()) {
    node = reinterpret_cast<Node *>(parent.internalPointer());
  }

  if (!node || 0 > row || row >= node->child_count) {
    return QModelIndex();
  }

  auto index = static_cast<size_t>(row);
  Node *child_node = nullptr;

  if (RootData *rd = std::get_if<RootData>(&(node->data))) {
    child_node = rd->children.at(index);

  } else if (CategoryData *cd = std::get_if<CategoryData>(&(node->data))) {
    child_node = cd->ordered_children.at(index);

  } else if (LocationData *ld = std::get_if<LocationData>(&(node->data))) {
    child_node = ld->children.at(index);
  }

  if (!child_node) {
    return QModelIndex();
  }

  return createIndex(row, column, child_node);
}

QModelIndex InformationExplorerModel::parent(const QModelIndex &child) const {
  if (!child.isValid()) {
    return QModelIndex();
  }

  Node *node = reinterpret_cast<Node *>(child.internalPointer());
  if (!node || !node->parent || node->parent == d->root) {
    return QModelIndex();
  }

  return createIndex(node->parent->row, 0, node->parent);
}

int InformationExplorerModel::rowCount(const QModelIndex &parent) const {
  Node *node = d->root;
  if (parent.isValid()) {
    node = reinterpret_cast<Node *>(parent.internalPointer());
  }

  if (!node) {
    return 0;
  }

  return node->child_count;
}

int InformationExplorerModel::columnCount(const QModelIndex &) const {
  return 1;
}

QVariant InformationExplorerModel::data(const QModelIndex &index,
                                        int role) const {
  QVariant ret;
  if (!index.isValid()) {
    return ret;
  }

  Node *node = reinterpret_cast<Node *>(index.internalPointer());
  if (!node) {
    return ret;
  }

  if (role == Qt::DisplayRole) {
    ret.setValue(node->display);

  } else if (role == LocationRole) {
    if (EntityData *ed = std::get_if<EntityData>(&(node->data))) {
      if (ed->location.has_value()) {
        ret.setValue(GetFileName(ed->location.value(), false));
      }
    }

  } else if (role == EntityIdRole) {
    if (EntityData *ed = std::get_if<EntityData>(&(node->data))) {
      ret.setValue(EntityId(ed->entity).Pack());
    }

  } else if (role == ForceTextPaintRole) {
    ret.setValue(!node->token_range);

  } else if (role == TokenRangeRole) {
    if (node->token_range) {
      ret.setValue(node->token_range);
    }

  } else if (role == AutoExpandRole) {
    ret.setValue(!std::holds_alternative<LocationData>(node->data));
  }

  return ret;
}

void InformationExplorerModel::InfoFutureResultStateChanged(void) {
  if (d->info_request_status_future.isCanceled()) {
    emit beginResetModel();
    ClearTree();
    emit endResetModel();
    return;
  }

  auto request_status = d->info_request_status_future.takeResult();
  if (!request_status) {
    emit beginResetModel();
    ClearTree();
    emit endResetModel();
    return;
  }

  // We got something.
  d->active_entity_id = d->next_active_entity_id;
  d->opt_active_entity_name = tr("CIAO CIAO");  // TODO xxxxxx
}

void InformationExplorerModel::NameFutureResultStateChanged(void) {
  if (d->info_request_status_future.isCanceled() ||
      d->name_request_future.isCanceled()) {
    return;
  }

  emit beginResetModel();

  auto entity_name_as_token = d->name_request_future.takeResult();
  auto entity_name_as_string_view = entity_name_as_token.data();
  d->opt_active_entity_name = QString::fromUtf8(
      entity_name_as_string_view.data(),
      static_cast<qsizetype>(entity_name_as_string_view.size()));

  emit endResetModel();
}

InformationExplorerModel::InformationExplorerModel(
    Index index, FileLocationCache file_location_cache, QObject *parent)
    : IInformationExplorerModel(parent),
      d(new PrivateData) {

  d->database = IDatabase::Create(index, file_location_cache);

  d->index = index;
  d->file_location_cache = file_location_cache;

  connect(&d->info_future_watcher, &QFutureWatcher<QFuture<bool>>::finished,
          this, &InformationExplorerModel::InfoFutureResultStateChanged);

  connect(&d->name_future_watcher, &QFutureWatcher<QFuture<bool>>::finished,
          this, &InformationExplorerModel::NameFutureResultStateChanged);

  connect(&d->import_timer, &QTimer::timeout, this,
          &InformationExplorerModel::ProcessDataBatchQueue);

  ClearTree();
}

void InformationExplorerModel::CancelRunningRequest() {
  d->import_timer.stop();

  if (d->info_request_status_future.isRunning()) {
    d->info_request_status_future.cancel();
    d->info_request_status_future.waitForFinished();
    d->info_request_status_future = {};
  }

  if (d->name_request_future.isRunning()) {
    d->name_request_future.cancel();
    d->name_request_future.waitForFinished();
    d->name_request_future = {};
  }
}

// NOTE(pag): This gets called in the context of the QFuture.
void InformationExplorerModel::OnDataBatch(DataBatch data_batch) {
  std::lock_guard<std::mutex> lock(d->data_batch_mutex);
  d->data_batch_queue.push_back(std::move(data_batch));
}

void InformationExplorerModel::ImportEntityInformation(EntityInformation &ei) {

  QString display_role = GetName(ei.display_role);
  if (display_role.isEmpty()) {
    display_role = GetFileName(ei.location, true /* path_only */);
    if (display_role.isEmpty()) {
      return;
    }
  }

  Node *root = d->root;
  RootData *root_data = &(std::get<RootData>(d->root->data));

  Node *category = nullptr;
  CategoryData *category_data = nullptr;

  Node *key = nullptr;
  Node *sub_category = nullptr;

  // Figure out if we have a top-level category into which to place this node.
  // There aren't that many top-level categories, so we linearly search for
  // one.
  for (Node *root_child : root_data->children) {
    category = root_child;
    category_data = &(std::get<CategoryData>(category->data));

    if (category_data->key == ei.category) {
      goto found_category;
    }
  }

  category = &(d->nodes.emplace_back());
  category->version = d->version;
  category->parent = root;
  category->row = static_cast<int>(root_data->children.size());
  category->data = CategoryData{};
  category->display = ei.category;
  category_data = &(std::get<CategoryData>(category->data));
  category_data->key = ei.category;
  root_data->children.push_back(category);

  d->OnNewNode(*this, category);

found_category:

  // We've found the top-level category. Now we want to place the node into
  // the category. We start by doing a text-based lookup on how the node data
  // will look. That's our way of deduplicating.
  key = category_data->keyed_children[display_role];

  // We didn't find anything that looks the same, so we'll add a new leaf node.
  if (!key) {
    EntityData ed;
    ed.entity = std::move(ei.entity_role);
    ed.location = ConvertLocation(ei.location);
    key = &(d->nodes.emplace_back());
    key->version = d->version;
    key->parent = category;
    key->row = static_cast<int>(category_data->ordered_children.size());
    key->data = std::move(ed);
    key->display = GetName(ei.display_role);
    key->token_range = GetTokenRange(ei.display_role);
    category_data->ordered_children.push_back(key);
    category_data->keyed_children[display_role] = key;
    d->OnNewNode(*this, key);

    // We've found a second thing that looks the same as another thing. We have
    // to change that other thing from being a leaf node to being a node that
    // has children.
  } else if (std::holds_alternative<EntityData>(key->data)) {
    sub_category = key;

    // Migrate the old data.
    key = &(d->nodes.emplace_back());
    key->version = d->version;
    key->parent = sub_category;
    key->row = 0;
    key->data = std::move(sub_category->data);

    QString key_loc = GetFileName(std::get<EntityData>(key->data).location,
                                  false /* path_only */);
    if (key_loc.isEmpty()) {
      key_loc = QString::number(
          EntityId(std::get<EntityData>(key->data).entity).Pack());
    }

    key->display = key_loc;

    // Convert the node type.
    sub_category->data = LocationData{};
    d->OnNodeChanged(*this, key);

    std::get<LocationData>(sub_category->data).children.push_back(key);
    d->OnNewNode(*this, key);

    goto add_to_sub_category;

    // We've found a third, fourth, etc. thing that looks the same as something
    // else.
  } else {
    sub_category = key;

  add_to_sub_category:

    // Add in the new data.
    QString key_loc = GetFileName(ei.location, false /* path_only */);
    if (key_loc.isEmpty()) {
      key_loc = QString::number(EntityId(ei.entity_role).Pack());
    }

    EntityData ed;
    ed.entity = std::move(ei.entity_role);
    ed.location = ConvertLocation(ei.location);

    key = &(d->nodes.emplace_back());
    key->version = d->version;
    key->parent = sub_category;
    key->row = 1;
    key->data = std::move(ed);
    key->display = std::move(key_loc);

    std::get<LocationData>(sub_category->data).children.push_back(key);
    d->OnNewNode(*this, key);
  }
}

void InformationExplorerModel::ProcessDataBatchQueue() {
  ++d->version;

  if (!d->info_request_status_future.isRunning()) {
    d->import_timer.stop();
  }

  std::vector<DataBatch> data_batch_queue;
  {
    std::lock_guard<std::mutex> lock(d->data_batch_mutex);
    data_batch_queue.swap(d->data_batch_queue);
  }

  d->active_entity_id = d->next_active_entity_id;

  if (data_batch_queue.empty()) {
    return;
  }

  // Imports the changes.
  for (DataBatch &data_batch : data_batch_queue) {
    for (EntityInformation &entity_information : data_batch) {
      ImportEntityInformation(entity_information);
    }
  }

  //  emit beginResetModel();
  // Exposes the changes.
  for (const Change &change : d->change_list) {
    QModelIndex parent_index;
    if (change.parent != d->root) {
      parent_index = createIndex(change.parent->row, 0, change.parent);
    }
    emit beginInsertRows(
        parent_index, change.parent->child_count,
        change.parent->child_count + change.num_children_added - 1);
    change.parent->child_count += change.num_children_added;
    emit endInsertRows();
  }

  d->change_list.clear();
  d->changes.clear();
  //  emit endResetModel();
}

}  // namespace mx::gui
