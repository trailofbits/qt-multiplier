/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "TreeExplorerModel.h"

#include <algorithm>
#include <multiplier/Token.h>

#include <QColor>
#include <QApplication>
#include <QFutureWatcher>
#include <QThreadPool>
#include <QTimer>
#include <QPalette>
#include <QtConcurrent>
#include <QDebug>

namespace mx::gui {

namespace {

const int kFirstUpdateInterval{500};
const int kImportInterval{1500};
const int kMaxBatchSize{250};

struct Node;

using TextAndTokenRange = std::pair<QString, TokenRange>;

using NodeData = std::variant<QString, TextAndTokenRange, QVariant>;

using NodeDataIterator = std::deque<NodeData>::iterator;

using NodeKey = std::pair<const RawEntityId, Node>;
using NodeKeyIterator = std::deque<NodeKey *>::iterator;

struct Node {
  NodeKey *parent_key{nullptr};
  int num_children{-1};  // -1 means unknown, 0 means none.
  NodeKeyIterator child_it;  // Iterator pointing to the first child.
  NodeKeyIterator sibling_it;  // Iterator pointing to the next sibling.
  NodeDataIterator data_it;

  inline Node(NodeKey *parent_key_)
      : parent_key(parent_key_) {}
};

struct DataBatch {
  NodeKey *parent_key;
  QList<std::shared_ptr<ITreeItem>> child_items;
  unsigned remaining_depth;

  inline DataBatch(NodeKey *parent_key_,
                   QList<std::shared_ptr<ITreeItem>> child_items_,
                   unsigned remaining_depth_)
      : parent_key(parent_key_),
        child_items(std::move(child_items_)),
        remaining_depth(remaining_depth_) {}
};

using DataBatchQueue = QList<DataBatch>;

}  // namespace

struct TreeExplorerModel::PrivateData final {

  // Data generator.
  std::shared_ptr<ITreeGenerator> generator;

  // The non-uniqued nodes of the tree.
  std::deque<NodeKey *> child_keys;

  // The redundant node keys. Sometimes we already have something in
  // `entity_to_node`, in which case, we leave it there, but we still make
  // a redundant node here in `redundant_keys` so that we can link together
  // things into a sibling list, but also know that we shouldn't actually
  // expand underneath something.
  std::deque<NodeKey> redundant_keys;

  // The data for all nodes.
  std::deque<NodeData> node_data;

  // The uniqued nodes of the tree.
  std::unordered_map<RawEntityId, Node> entity_to_node;

  // The "root" node. Its children represent the top-level items in the tree.
  Node root_node;

  // Number of columns.
  int num_columns{0};

  // Returns the number of pending requests.
  int num_pending_requests{0};

  // Version number of this model. This is incremented when we install a new
  // generator.
  uint64_t version_number{0u};

  //! Future used to resolve the name of the tree.
  QFuture<QString> tree_name_future;
  QFutureWatcher<QString> tree_name_future_watcher;

  //! A timer used to import data from the data batch queue
  QTimer import_timer;

  // Queue of groups of children `ITreeItem`s to insert into the model.
  DataBatchQueue data_batch_queue;

  inline PrivateData(void)
      : root_node(nullptr) {}

  NodeKey *NodeKeyFrom(const QModelIndex &index) const {
    if (!index.isValid()) {
      return nullptr;
    }

    return const_cast<NodeKey *>(reinterpret_cast<const NodeKey *>(
        index.internalPointer()));
  }

  std::pair<RawEntityId, Node *> NodeFrom(const QModelIndex &index) const {
    auto ret = NodeKeyFrom(index);
    if (!ret) {
      return {kInvalidEntityId, nullptr};
    }

    return {ret->first, &(ret->second)};
  }

  NodeKey *NodeKeyFromId(RawEntityId entity_id) {
    auto it = entity_to_node.find(entity_id);
    if (it == entity_to_node.end()) {
      return nullptr;
    }
    return &*it;
  }

  Node *NodeFromId(RawEntityId entity_id) {
    if (auto key = NodeKeyFromId(entity_id)) {
      return &(key->second);
    }
    return nullptr;
  }

  void ImportData(Node *new_node, const std::shared_ptr<ITreeItem> &item);
};

//! Constructor
TreeExplorerModel::TreeExplorerModel(QObject *parent)
    : ITreeExplorerModel(parent),
      d(new PrivateData) {

  connect(&(d->tree_name_future_watcher),
          &QFutureWatcher<QFuture<QString>>::finished,
          this, &TreeExplorerModel::OnNameResolved);

  connect(&d->import_timer, &QTimer::timeout,
          this, &TreeExplorerModel::ProcessDataBatchQueue);
}

TreeExplorerModel::~TreeExplorerModel() {
  CancelRunningRequest();
}

void TreeExplorerModel::RunExpansionThread(
    ITreeExplorerExpansionThread *expander) {

  connect(expander, &ITreeExplorerExpansionThread::NewTreeItems,
          this, &TreeExplorerModel::OnNewTreeItems);

  if (!d->num_pending_requests) {
    qDebug() << "Started timer";
    d->import_timer.start(kFirstUpdateInterval);
    emit RequestStarted();
  }

  d->num_pending_requests += 1;

  qDebug() << "Started expander" << d->num_pending_requests;
  QThreadPool::globalInstance()->start(expander);
}

//! Install a new generator to back the data of this model.
void TreeExplorerModel::InstallGenerator(
    std::shared_ptr<ITreeGenerator> generator_) {

  CancelRunningRequest();

  emit beginResetModel();
  d->version_number++;
  d->num_pending_requests = 0;
  d->generator = std::move(generator_);
  d->node_data.clear();
  d->entity_to_node.clear();
  d->child_keys.clear();
  d->redundant_keys.clear();
  d->num_columns = d->generator->NumColumns();
  d->root_node.parent_key = nullptr;
  d->root_node.data_it = d->node_data.end();
  d->root_node.child_it = d->child_keys.end();
  d->root_node.sibling_it = d->child_keys.end();
  d->root_node.num_children = -1;
  d->import_timer.stop();
  d->data_batch_queue.clear();
  emit endResetModel();

  // Start a request to fetch the name of this tree.
  d->tree_name_future = QtConcurrent::run([gen = d->generator] (void) -> QString {
    return gen->TreeName(gen);
  });
  d->tree_name_future_watcher.setFuture(d->tree_name_future);

  // Go load up the roots.
  RunExpansionThread(new InitTreeExplorerThread(
      d->generator, d->version_number, kInvalidEntityId, 1u));
}

void TreeExplorerModel::ExpandEntity(const QModelIndex &index,
                                     unsigned depth) {
  if (!depth) {
    return;
  }

  auto [entity_id, entity] = d->NodeFrom(index);
  if (!entity || -1 != entity->num_children) {
    return;
  }

  RunExpansionThread(new ExpandTreeExplorerThread(
      d->generator, d->version_number, entity_id, depth));
}

QModelIndex TreeExplorerModel::index(int row, int column,
                                     const QModelIndex &parent) const {
  if (!hasIndex(row, column, parent)) {
    return QModelIndex();
  }

  Node *entity = nullptr;
  if (auto entity_key = d->NodeKeyFrom(parent)) {
    entity = &(entity_key->second);  // Internal node.
  } else {
    entity = &(d->root_node);
  }

  if (-1 == entity->num_children || row >= entity->num_children) {
    return QModelIndex();
  }

  NodeKey *child_key = *(entity->child_it + row);
  return createIndex(row, column, static_cast<const void *>(child_key));
}

QModelIndex TreeExplorerModel::parent(const QModelIndex &child) const {
  auto [entity_id, entity] = d->NodeFrom(child);
  if (!entity || !entity->parent_key) {
    return QModelIndex();
  }

  NodeKey *parent_key = entity->parent_key;
  Node *parent_node = &(entity->parent_key->second);
  Node *grandparent_node = parent_node->parent_key ?
                           &(parent_node->parent_key->second) :
                           &(d->root_node);

  // Figure out the position of the parent among its siblings.
  return createIndex(
      static_cast<int>(entity->sibling_it - grandparent_node->child_it) - 1,
      0,
      reinterpret_cast<const void *>(parent_key));
}

int TreeExplorerModel::rowCount(const QModelIndex &parent) const {
  if (parent.column() > 0) {
    return 0;
  }

  int row_count = d->root_node.num_children;
  if (parent.isValid()) {
    row_count = -1;
    if (auto entity = &(d->NodeKeyFrom(parent)->second)) {
      row_count = entity->num_children;
    }
  }

  return std::max(0, row_count);
}

int TreeExplorerModel::columnCount(const QModelIndex &) const {
  return d->num_columns;
}

QVariant TreeExplorerModel::headerData(int section,
                                       Qt::Orientation orientation,
                                       int role) const {

  if (orientation != Qt::Horizontal || role != Qt::DisplayRole ||
      section < 0 || section >= d->num_columns) {
    return QVariant();
  }

  return d->generator->ColumnTitle(section);
}

QVariant TreeExplorerModel::data(const QModelIndex &index, int role) const {
  QVariant value;

  NodeKey *entity_key = d->NodeKeyFrom(index);
  if (!entity_key) {
    return value;
  }

  Node *entity = &(entity_key->second);

  auto data_it = entity->data_it + index.column();

  if (role == Qt::DisplayRole) {
    if (std::holds_alternative<QString>(*data_it)) {
      value.setValue(std::get<QString>(*data_it));

    } else if (std::holds_alternative<TextAndTokenRange>(*data_it)) {
      value.setValue(std::get<TextAndTokenRange>(*data_it).first);

    } else {
      value = std::get<QVariant>(*data_it);
    }
  } else if (role == ITreeExplorerModel::EntityIdRole) {
    value.setValue(entity_key->first);

  } else if (role == ITreeExplorerModel::TokenRangeRole) {
    if (std::holds_alternative<TextAndTokenRange>(*data_it)) {
      value.setValue(std::get<TextAndTokenRange>(*data_it).second);
    }
  
  } else if (role == ITreeExplorerModel::HasBeenExpanded) {
    value.setValue(-1 != entity->num_children); 

  } else if (role == ITreeExplorerModel::IsDuplicate) {
    value.setValue(entity_key != index.internalPointer());
  }

  return value;
}

//! Called when the tree title has been resolved.
void TreeExplorerModel::OnNameResolved(void) {
  if (d->tree_name_future.isCanceled()) {
    return;
  }

  emit TreeNameChanged(d->tree_name_future.takeResult());
}

void TreeExplorerModel::CancelRunningRequest() {
  d->tree_name_future.cancel();
  d->tree_name_future.waitForFinished();
  d->tree_name_future = {};

  if (!d->num_pending_requests && d->data_batch_queue.empty()) {
    return;
  }

  d->version_number++;
  d->num_pending_requests = 0;
  d->import_timer.stop();
  d->data_batch_queue.clear();
  emit RequestFinished();
}

//! Notify us when there's a batch of new data to update.
void TreeExplorerModel::OnNewTreeItems(
    uint64_t version_number, RawEntityId parent_entity_id,
    QList<std::shared_ptr<ITreeItem>> child_items, unsigned remaining_depth) {

  if (version_number != d->version_number) {
    return;
  }

  d->num_pending_requests -= 1;

  qDebug() << "Finished expander" << d->num_pending_requests;

  d->data_batch_queue.emplaceBack(
      d->NodeKeyFromId(parent_entity_id), std::move(child_items), remaining_depth);
}


// Go get all of our data for this node.
void TreeExplorerModel::PrivateData::ImportData(
    Node *new_node, const std::shared_ptr<ITreeItem> &item) {
  auto has_first_data = false;

  for (auto i = 0; i < num_columns; ++i) {
    QVariant col_data = item->Data(i);
    if (col_data.canConvert<QString>()) {
      node_data.emplace_back(col_data.toString());

    } else if (col_data.canConvert<TokenRange>()) {
      TokenRange tok_range = col_data.value<TokenRange>();
      auto char_data = tok_range.data();
      node_data.emplace_back(TextAndTokenRange{
          QString::fromUtf8(char_data.data(),
                            static_cast<qsizetype>(char_data.size())),
          std::move(tok_range)});

    } else {
      node_data.emplace_back(std::move(col_data));
    }

    // Link the node's data iterator to the data itself.
    if (!has_first_data) {
      has_first_data = true;
      new_node->data_it = node_data.end() - 1;
    }
  }

  if (!has_first_data) {
    new_node->data_it = node_data.end();
  }
}

void TreeExplorerModel::ProcessDataBatchQueue() {

  // Recursive requests for loading more items.
  std::vector<std::pair<NodeKey *, unsigned>> load_keys;

  int num_imported = 0;
  Node * const root_entity = &(d->root_node);

  qDebug() << "Processing data batch";

  while (!d->data_batch_queue.empty()) {
    DataBatch batch = std::move(d->data_batch_queue.front());
    d->data_batch_queue.pop_front();

    qDebug() << "Got batch";

    // The parent for this batch already has children.
    Node *parent_entity = nullptr;

    QModelIndex parent_index;
    if (batch.parent_key) {
      parent_entity = &(batch.parent_key->second);

      // Figure out the iterator pointing to the sibling list of the parent.
      NodeKeyIterator sibling_it = parent_entity->parent_key ?
                                   parent_entity->parent_key->second.child_it :
                                   root_entity->child_it;
      parent_index = createIndex(
          static_cast<int>(parent_entity->sibling_it - sibling_it) - 1, 0,
          reinterpret_cast<const void *>(batch.parent_key));

    } else {
      parent_entity = root_entity;
    }

    int * const num_children = &(parent_entity->num_children);

    // We've already loaded the children for this parent.
    if (*num_children != -1) {
      qDebug() << "Batch is redundant; children already fetched";
      continue;
    }

    // Update the number of children of the parent.
    *num_children = static_cast<int>(batch.child_items.size());

    // No children were found; mark the parent as having no children.
    if (!*num_children) {
      qDebug() << "Batch is empty";
      continue;
    }

    qDebug() << parent_index << 0 << (*num_children - 1);
    emit beginInsertRows(parent_index, 0, *num_children - 1);

    NodeKeyIterator *children_it = &(parent_entity->child_it);

    for (auto &item : batch.child_items) {

      qDebug() << "Processing item";

      ++num_imported;

      RawEntityId eid = item->EntityId();

      // Now create the node key. If this is the first time we're seeing the
      // node, then the node key is in our `entity_to_node` map; otherwise we
      // make a redundant key in `redundant_keys`.
      NodeKey *curr_key = nullptr;
      auto [node_it, added] = d->entity_to_node.try_emplace(eid, batch.parent_key);
      NodeKey *load_key = &*node_it;
      if (added) {
        curr_key = load_key;
      } else {
        curr_key = &(d->redundant_keys.emplace_back(eid, Node{batch.parent_key}));
      }

      Node * const new_node = &(curr_key->second);

      // Queue up recursive load requests for children.
      if (batch.remaining_depth && load_key->second.num_children == -1) {
        load_keys.emplace_back(load_key, batch.remaining_depth);
      }

      d->child_keys.emplace_back(curr_key);
      *children_it = d->child_keys.end() - 1;
      children_it = &(curr_key->second.sibling_it);

      // If this is a new node, then import the data, otherwise reference the
      // existing data.
      if (load_key == curr_key) {
        d->ImportData(new_node, item);
      } else {
        new_node->data_it = load_key->second.data_it;
      }
    }

    // End each list of children with a dummy node, so that we can use the
    // sibling iterator to get a node's index. 
    d->child_keys.emplace_back(nullptr);
    *children_it = d->child_keys.end() - 1;

    emit endInsertRows();

    if (kMaxBatchSize >= num_imported) {
      break;
    }
  }

  bool has_remaining = !d->data_batch_queue.empty() || !load_keys.empty() ||
                       0 < d->num_pending_requests;

  // Queue up workers to go and recursively expand.
  for (auto [new_parent, remaining_depth] : load_keys) {
    RunExpansionThread(new ExpandTreeExplorerThread(
        d->generator, d->version_number, new_parent->first, remaining_depth));
  }

  // Restart the timer, so that the import procedure will fire again
  // in kImportInterval msecs from the end of the previous batch.
  if (has_remaining) {
    if (!d->import_timer.isActive()) {
      qDebug() << "Restarting timer";
      d->import_timer.start(kImportInterval);
    }

  } else {
    qDebug() << "Stopping timer";
    d->import_timer.stop();
    emit RequestFinished();
  }
}

struct ITreeExplorerExpansionThread::PrivateData {
  const std::shared_ptr<ITreeGenerator> generator;
  const uint64_t version_number;
  const RawEntityId parent_entity_id;
  const unsigned depth;

  inline PrivateData(std::shared_ptr<ITreeGenerator> generator_,
                     uint64_t version_number_, RawEntityId parent_entity_id_,
                     unsigned depth_)
      : generator(std::move(generator_)),
        version_number(version_number_),
        parent_entity_id(parent_entity_id_),
        depth(depth_) {}
};

ITreeExplorerExpansionThread::~ITreeExplorerExpansionThread(void) {}

ITreeExplorerExpansionThread::ITreeExplorerExpansionThread(
    std::shared_ptr<ITreeGenerator> generator_,
    uint64_t version_number, RawEntityId parent_entity_id,
    unsigned depth)
    : d(new PrivateData(std::move(generator_), version_number,
                        parent_entity_id, depth)) {
  setAutoDelete(true);
}

void InitTreeExplorerThread::run(void) {
  QList<std::shared_ptr<ITreeItem>> items;
  for (auto item : d->generator->Roots(d->generator)) {
    qDebug() << "Generated root item";
    items.emplaceBack(std::move(item));
  }
  emit NewTreeItems(
      d->version_number, d->parent_entity_id, items, d->depth - 1u);
}

void ExpandTreeExplorerThread::run(void) {
  QList<std::shared_ptr<ITreeItem>> items;
  for (auto item : d->generator->Children(d->generator, d->parent_entity_id)) {
    items.emplaceBack(std::move(item));
  }
  emit NewTreeItems(
      d->version_number, d->parent_entity_id, items, d->depth - 1u);
}

}  // namespace mx::gui
