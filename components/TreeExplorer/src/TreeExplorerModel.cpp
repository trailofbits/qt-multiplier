/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "TreeExplorerModel.h"
#include "InitTreeExplorerThread.h"
#include "ExpandTreeExplorerThread.h"

#include <multiplier/Token.h>

#include <QColor>
#include <QApplication>
#include <QFutureWatcher>
#include <QThreadPool>
#include <QTimer>
#include <QPalette>
#include <QtConcurrent>
#include <QDebug>

#include <algorithm>
#include <cassert>

namespace mx::gui {

namespace {

const int kFirstUpdateInterval{500};
const int kImportInterval{1500};
const int kMaxBatchSize{100};

struct Node;

using TextAndTokenRange = std::pair<QString, TokenRange>;
using NodeData = std::variant<QString, TextAndTokenRange, QVariant>;
using NodeKey = std::pair<const RawEntityId, Node>;

enum class NodeState { kUnopened, kOpening, kOpened, kDuplicate };

struct Node {
  // Pointer to this node's parent.
  NodeKey *parent_key{nullptr};

  // The current state of this node. By default, all nodes are unopened, i.e.
  // we haven't tried to fetch their children. Some nodes are marked as
  // duplicates.
  NodeState state{NodeState::kUnopened};

  // The number of children of this node.
  int num_children{0};

  // Index into `d->child_keys` of the first child of this node, if
  // `num_children` is greater than zero. The last child can be found at
  // `d->child_keys[d->child_index + num_children - 1]`.
  unsigned child_index{0u};

  // Index into `d->child_keys` of the next item sharing `d->parent_key`.
  unsigned sibling_index{0u};

  // Index into `d->node_data`. `d->node_data[d->data_index]` is the data for
  // first column, `d->node_data[d->data_index + 1]` for the second, ..., and
  // `d->node_data[d->data_index + d->num_columns - 1]` for the last column.
  unsigned data_index{0u};

  // Index into `d->child_keys`. If this node isn't a duplicate, then this index
  // will reference back to the node itself. Otherwise it will reference the
  // first/original node.
  unsigned alias_index{0u};

  inline Node(NodeKey *parent_key_) : parent_key(parent_key_) {}
};

struct DataBatch {
  NodeKey *parent_key;
  QList<std::shared_ptr<ITreeItem>> child_items;
  unsigned remaining_depth;
  unsigned *index_ptr{nullptr};

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

  // Used to help deduplicate.
  std::unordered_map<RawEntityId, NodeKey *> aliased_entity_to_key;

  // The "root" node. Its children represent the top-level items in the tree.
  Node root_node;

  // Number of columns.
  int num_columns{0};

  // Returns the number of pending requests.
  int num_pending_requests{0};

  // Version number of this model. This is incremented when we install a new
  // generator.
  const VersionNumber version_number;

  //! Future used to resolve the name of the tree.
  QFuture<QString> tree_name_future;
  QFutureWatcher<QString> tree_name_future_watcher;

  //! A timer used to import data from the data batch queue
  QTimer import_timer;

  // Queue of groups of children `ITreeItem`s to insert into the model.
  DataBatchQueue data_batch_queue;

  inline PrivateData(void)
      : root_node(nullptr),
        version_number(std::make_shared<std::atomic<uint64_t>>()) {}

  NodeKey *NodeKeyFrom(const QModelIndex &index) const {
    if (!index.isValid()) {
      return nullptr;
    }

    return const_cast<NodeKey *>(
        reinterpret_cast<const NodeKey *>(index.internalPointer()));
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
          &QFutureWatcher<QFuture<QString>>::finished, this,
          &TreeExplorerModel::OnNameResolved);

  connect(&d->import_timer, &QTimer::timeout, this,
          &TreeExplorerModel::ProcessDataBatchQueue);
}

TreeExplorerModel::~TreeExplorerModel() {
  CancelRunningRequest();
}

void TreeExplorerModel::RunExpansionThread(
    ITreeExplorerExpansionThread *expander) {

  connect(expander, &ITreeExplorerExpansionThread::NewTreeItems, this,
          &TreeExplorerModel::OnNewTreeItems);

  if (!d->num_pending_requests) {
    d->import_timer.start(kFirstUpdateInterval);
    emit RequestStarted();
  }

  d->num_pending_requests += 1;

  QThreadPool::globalInstance()->start(expander);
}

//! Install a new generator to back the data of this model.
void TreeExplorerModel::InstallGenerator(
    std::shared_ptr<ITreeGenerator> generator_) {

  CancelRunningRequest();

  emit beginResetModel();
  d->version_number->fetch_add(1u);
  d->num_pending_requests = 0;
  d->generator = std::move(generator_);
  d->node_data.clear();
  d->entity_to_node.clear();
  d->aliased_entity_to_key.clear();
  d->child_keys.clear();
  d->redundant_keys.clear();
  d->num_columns = d->generator->NumColumns();
  d->root_node.parent_key = nullptr;
  d->root_node.data_index = 0u;
  d->root_node.child_index = 0u;
  d->root_node.sibling_index = 0u;
  d->root_node.num_children = 0;
  d->root_node.state = NodeState::kUnopened;
  d->import_timer.stop();
  d->data_batch_queue.clear();
  emit endResetModel();

  // Start a request to fetch the name of this tree.
  d->tree_name_future = QtConcurrent::run(
      [gen = d->generator](void) -> QString { return gen->TreeName(gen); });
  d->tree_name_future_watcher.setFuture(d->tree_name_future);

  // Go load up the roots.
  d->root_node.state = NodeState::kOpening;
  RunExpansionThread(new InitTreeExplorerThread(d->generator, d->version_number,
                                                kInvalidEntityId, 2u));
}

void TreeExplorerModel::ExpandEntity(const QModelIndex &index, unsigned depth) {
  if (!depth) {
    return;
  }

  auto [entity_id, entity] = d->NodeFrom(index);
  if (!entity || NodeState::kUnopened != entity->state) {
    return;
  }

  entity->state = NodeState::kOpening;
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

  if (NodeState::kUnopened == entity->state || row >= entity->num_children) {
    return QModelIndex();
  }

  NodeKey *child_key =
      d->child_keys[entity->child_index + static_cast<unsigned>(row)];
  return createIndex(row, column, static_cast<const void *>(child_key));
}

QModelIndex TreeExplorerModel::parent(const QModelIndex &child) const {
  auto [entity_id, entity] = d->NodeFrom(child);
  if (!entity || !entity->parent_key) {
    return QModelIndex();
  }

  NodeKey *parent_key = entity->parent_key;

  assert(parent_key != nullptr);
  Node *parent_node = &(parent_key->second);
  Node *grandparent_node = parent_node->parent_key
                               ? &(parent_node->parent_key->second)
                               : &(d->root_node);

  // Figure out the position of the parent among its siblings.
  return createIndex(
      static_cast<int>(entity->sibling_index - grandparent_node->child_index) -
          1,
      0, reinterpret_cast<const void *>(parent_key));
}

int TreeExplorerModel::rowCount(const QModelIndex &parent) const {
  if (parent.column() > 0) {
    return 0;
  }

  int row_count = d->root_node.num_children;
  if (parent.isValid()) {
    row_count = 0;
    if (auto entity = &(d->NodeKeyFrom(parent)->second)) {
      row_count = entity->num_children;
    }
  }

  return row_count;
}

int TreeExplorerModel::columnCount(const QModelIndex &) const {
  return d->num_columns;
}

QVariant TreeExplorerModel::headerData(int section, Qt::Orientation orientation,
                                       int role) const {

  if (orientation != Qt::Horizontal || role != Qt::DisplayRole || section < 0 ||
      section >= d->num_columns) {
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

  const NodeData &data =
      d->node_data[entity->data_index + static_cast<unsigned>(index.column())];

  if (role == Qt::DisplayRole) {
    if (std::holds_alternative<QString>(data)) {
      value.setValue(std::get<QString>(data));

    } else if (std::holds_alternative<TextAndTokenRange>(data)) {
      value.setValue(std::get<TextAndTokenRange>(data).first);

    } else {
      value = std::get<QVariant>(data);
    }

    // Tooltip used for hovering. Also, this is used for the copy details.
  } else if (role == Qt::ToolTipRole) {
    QString tooltip = tr("Entity id: ") + QString::number(entity_key->first);

    for (int i = 0; i < d->num_columns; ++i) {
      const NodeData &col_data =
          d->node_data[entity->data_index + static_cast<unsigned>(i)];
      if (std::holds_alternative<QVariant>(data)) {
        continue;
      }

      tooltip += "\n" + d->generator->ColumnTitle(i) + ": ";
      if (std::holds_alternative<QString>(col_data)) {
        tooltip += std::get<QString>(col_data);

      } else if (std::holds_alternative<TextAndTokenRange>(col_data)) {
        tooltip += std::get<TextAndTokenRange>(col_data).first;
      }
    }
    value.setValue(tooltip);

  } else if (role == ITreeExplorerModel::EntityIdRole) {
    value.setValue(entity_key->first);

  } else if (role == ITreeExplorerModel::TokenRangeRole) {
    if (std::holds_alternative<TextAndTokenRange>(data)) {
      value.setValue(std::get<TextAndTokenRange>(data).second);
    }

  } else if (role == ITreeExplorerModel::CanBeExpanded) {
    value.setValue(NodeState::kUnopened == entity->state);

  } else if (role == ITreeExplorerModel::IsDuplicate) {
    value.setValue(NodeState::kDuplicate == entity->state);
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

  d->version_number->fetch_add(1u);
  d->num_pending_requests = 0;
  d->import_timer.stop();
  d->data_batch_queue.clear();
  emit RequestFinished();
}

//! Notify us when there's a batch of new data to update.
void TreeExplorerModel::OnNewTreeItems(
    uint64_t version_number, RawEntityId parent_entity_id,
    QList<std::shared_ptr<ITreeItem>> child_items, unsigned remaining_depth) {

  if (version_number != d->version_number->load()) {
    return;
  }

  d->num_pending_requests -= 1;
  d->data_batch_queue.emplaceBack(d->NodeKeyFromId(parent_entity_id),
                                  std::move(child_items), remaining_depth);
}

// Go get all of our data for this node.
void TreeExplorerModel::PrivateData::ImportData(
    Node *new_node, const std::shared_ptr<ITreeItem> &item) {

  new_node->data_index = static_cast<unsigned>(node_data.size());

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
  }
}

void TreeExplorerModel::ProcessDataBatchQueue() {

  // Recursive requests for loading more items.
  std::vector<std::pair<NodeKey *, unsigned>> load_keys;

  // Count how many items we've imported so that we can batch them across
  // timer events.
  int num_imported = 0;

  Node *const root_entity = &(d->root_node);

  while (!d->data_batch_queue.empty()) {
    DataBatch &batch = d->data_batch_queue.front();

    // The parent for this batch already has children.
    Node *parent_entity = nullptr;

    QModelIndex parent_index;
    if (batch.parent_key) {
      parent_entity = &(batch.parent_key->second);

      // Figure out the iterator pointing to the sibling list of the parent.
      unsigned sibling_index =
          parent_entity->parent_key
              ? parent_entity->parent_key->second.child_index
              : root_entity->child_index;

      parent_index = createIndex(
          static_cast<int>(parent_entity->sibling_index - sibling_index) - 1, 0,
          reinterpret_cast<const void *>(batch.parent_key));

    } else {
      parent_entity = root_entity;
    }

    // We've already loaded the children for this parent.
    if (NodeState::kOpening != parent_entity->state) {
      continue;
    }

    // No children were found; mark the parent as having no children.
    if (batch.child_items.empty()) {
      parent_entity->state = NodeState::kOpened;
      d->data_batch_queue.pop_front();
      continue;
    }

    // If we already have this pointer, then we're resuming adding children
    // to `parent_entity` after we previous hit our batch size limit and
    // deferred further adding until another timer interval.
    if (!batch.index_ptr) {
      batch.index_ptr = &(parent_entity->child_index);
    }

    int num_imported_children = 0;
    while (!batch.child_items.empty()) {

      auto item = std::move(batch.child_items.front());
      batch.child_items.pop_front();

      const RawEntityId eid = item->EntityId();
      if (eid == kInvalidEntityId) {
        continue;
      }

      // Now create the node key. If this is the first time we're seeing the
      // node, then the node key is in our `entity_to_node` map; otherwise we
      // make a redundant key in `redundant_keys`.
      NodeKey *curr_key = nullptr;
      auto [node_it, added] =
          d->entity_to_node.try_emplace(eid, batch.parent_key);
      NodeKey *load_key = &*node_it;
      if (added) {
        curr_key = load_key;

        // Even though this is a new node, link it to a prior node. This is to
        // allow us to reprepresent another form of equivalence to the
        // deduplication mechanism, i.e. that one declaration may be a
        // redeclaration of another one.
        const RawEntityId aliased_eid = item->AliasedEntityId();
        if (aliased_eid != kInvalidEntityId && aliased_eid != eid) {
          NodeKey *&alias_key = d->aliased_entity_to_key[aliased_eid];

          if (!alias_key) {
            auto alias_it = d->entity_to_node.find(aliased_eid);
            if (alias_it != d->entity_to_node.end()) {
              load_key = &*alias_it;
              alias_key = load_key;
            } else {
              alias_key = curr_key;  // Store for future dedup.
            }
          } else {
            load_key = alias_key;
          }

          // An existing thing notifies us of this alias.
        } else if (auto alias_it = d->aliased_entity_to_key.find(eid);
                   alias_it != d->aliased_entity_to_key.end()) {
          load_key = alias_it->second;
        }

      } else {
        curr_key =
            &(d->redundant_keys.emplace_back(eid, Node{batch.parent_key}));
      }

      Node *const new_node = &(curr_key->second);

      if (load_key != curr_key) {
        new_node->state = NodeState::kDuplicate;
      }

      // Queue up recursive load requests for children.
      if (batch.remaining_depth &&
          NodeState::kUnopened == load_key->second.state) {
        load_keys.emplace_back(load_key, batch.remaining_depth);
      }

      // Make the node point to itself, and update the parent child index or
      // previous sibling's next sibling index.
      new_node->alias_index = static_cast<unsigned>(d->child_keys.size());
      *(batch.index_ptr) = new_node->alias_index;

      // Possibly make the node point to its alias.
      new_node->alias_index = load_key->second.alias_index;

      d->child_keys.emplace_back(curr_key);
      batch.index_ptr = &(curr_key->second.sibling_index);

      // If this is a new node, then import the data, otherwise reference the
      // existing data.
      if (added) {
        d->ImportData(new_node, item);
      } else {
        new_node->data_index = load_key->second.data_index;
      }

      ++num_imported_children;
      ++num_imported;

      if (kMaxBatchSize <= num_imported) {
        break;
      }
    }

    // We didn't end up importing anything.
    if (!num_imported_children) {
      parent_entity->state = NodeState::kOpened;
      d->data_batch_queue.pop_front();
      continue;
    }

    // Update the number of children of the parent.
    emit beginInsertRows(
        parent_index, parent_entity->num_children,
        parent_entity->num_children + num_imported_children - 1);

    parent_entity->num_children += num_imported_children;

    // End each list of children with a dummy node, so that we can use the
    // sibling iterator to get a node's index.
    *(batch.index_ptr) = static_cast<unsigned>(d->child_keys.size());

    emit endInsertRows();

    if (batch.child_items.empty()) {
      parent_entity->state = NodeState::kOpened;
      d->data_batch_queue.pop_front();
    }

    if (kMaxBatchSize <= num_imported) {
      break;
    }
  }

  bool has_remaining = !d->data_batch_queue.empty() || !load_keys.empty() ||
                       0 < d->num_pending_requests;

  // Queue up workers to go and recursively expand.
  for (auto [new_parent, remaining_depth] : load_keys) {
    if (NodeState::kUnopened == new_parent->second.state) {
      new_parent->second.state = NodeState::kOpening;
      RunExpansionThread(new ExpandTreeExplorerThread(
          d->generator, d->version_number, new_parent->first, remaining_depth));
    }
  }

  // Restart the timer, so that the import procedure will fire again
  // in kImportInterval msecs from the end of the previous batch.
  if (has_remaining) {
    if (!d->import_timer.isActive()) {
      d->import_timer.start(kImportInterval);
    }

  } else {
    d->import_timer.stop();
    emit RequestFinished();
  }
}


}  // namespace mx::gui
