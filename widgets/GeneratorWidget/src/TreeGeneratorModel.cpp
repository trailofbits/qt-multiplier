/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "TreeGeneratorModel.h"

#include <QColor>
#include <QApplication>
#include <QThreadPool>
#include <QTimer>
#include <QPalette>
#include <QtConcurrent>
#include <QDebug>

#include <atomic>
#include <algorithm>
#include <cassert>
#include <deque>
#include <multiplier/GUI/Interfaces/ITreeGenerator.h>
#include <multiplier/GUI/Util.h>
#include <unordered_map>

#include "InitTreeRunnable.h"
#include "ExpandTreeRunnable.h"

namespace mx::gui {
namespace {

struct Node;
using NodePtrList = std::vector<Node *>;

struct Node {
  IGeneratedItemPtr item;

  // Parent node.
  Node *parent{nullptr};

  // List of child nodes.
  NodePtrList nodes;

  // Index of this node within `parent`.
  int row{0};

  // When unopened, this is `nullptr`. When opened, this is itself. When it's
  // a duplicate, it points to the duplicate.
  Node *self_or_duplicate{nullptr};

  Node *Deduplicate(void) {
    auto node = this;
    if (self_or_duplicate) {
      while (node->self_or_duplicate && node->self_or_duplicate != node) {
        node = node->self_or_duplicate;
      }
      self_or_duplicate = node;
    }
    return node;
  }
};

struct QueuedItem {
  uint64_t version_number;
  RawEntityId parent_entity_id;
  IGeneratedItemPtr item;
  unsigned remaining_depth;
};

}  // namespace

struct TreeGeneratorModel::PrivateData final {
  //! Root node of our tree.
  Node root;

  //! The uniqued nodes of the tree.
  std::unordered_map<RawEntityId, Node *> entity_to_node;

  //! Time that decides when we should next do an import of items into a tree.
  QTimer import_timer;
  
  //! Queue of generated data to insert into our trees. The data are triples of
  //! the version number, the parent node pointer, and the item itself.
  std::list<QueuedItem> insertion_queue;

  //! Data generator.
  ITreeGeneratorPtr generator;

  //! All nodes of the tree.
  std::deque<Node> nodes;

  //! Number of columns.
  int num_columns{0};

  //! Returns the number of pending requests.
  int num_pending_requests{0};

  //! Version number of this model. This is incremented when we install a new
  //! generator.
  std::atomic_uint64_t version_number;

  //! Thread pool for all expansion runnables.
  QThreadPool thread_pool;
};

//! Constructor
TreeGeneratorModel::TreeGeneratorModel(QObject *parent)
    : IModel(parent),
      d(new PrivateData) {

  connect(&d->import_timer, &QTimer::timeout, this,
          &TreeGeneratorModel::ProcessData);
}

TreeGeneratorModel::~TreeGeneratorModel(void) {
  CancelRunningRequest();
}

void TreeGeneratorModel::RunExpansionThread(
    IGenerateTreeRunnable *runnable) {

  connect(runnable, &IGenerateTreeRunnable::NewGeneratedItems,
          this, &TreeGeneratorModel::AddData);

  connect(runnable, &IGenerateTreeRunnable::Finished,
          this, &TreeGeneratorModel::OnRequestFinished);

  if (!d->num_pending_requests) {
    emit RequestStarted();
  }

  d->num_pending_requests += 1;
  d->thread_pool.start(runnable);
}

void TreeGeneratorModel::OnRequestFinished(void) {
  d->num_pending_requests -= 1;
  Q_ASSERT(d->num_pending_requests >= 0);
  if (!d->num_pending_requests) {
    emit RequestFinished();
  }
}

//! Find the original version of an item.
QModelIndex TreeGeneratorModel::Deduplicate(const QModelIndex &index) {
  if (!index.isValid()) {
    return {};
  }

  Node *node = reinterpret_cast<Node *>(index.internalPointer());
  if (!node) {
    return {};
  }

  node = node->Deduplicate();
  return createIndex(node->row, index.column(), node);
}

//! Expand starting at the model index, going up to `depth` levels deep.
void TreeGeneratorModel::Expand(const QModelIndex &index, unsigned depth) {

  if (!depth) {
    return;
  }

  if (!index.isValid()) {
    return;
  }

  Node *node = reinterpret_cast<Node *>(index.internalPointer());
  if (!node) {
    return;
  }

  // Initialize a worklist.
  std::vector<std::pair<Node *, unsigned>> todo;
  todo.emplace_back(node, depth);

  // Run through the worklist to recursively expand.
  for (auto i = 0ul; i < todo.size(); ++i) {
    node = todo[i].first->Deduplicate();
    depth = todo[i].second;

    // Never been expanded; try to expand it.
    if (!node->self_or_duplicate) {
      node->self_or_duplicate = node;
      RunExpansionThread(new ExpandTreeRunnable(
          d->generator, d->version_number, node->item, depth));
      continue;
    }

    Q_ASSERT(node->self_or_duplicate == node);

    if (auto next_depth = depth - 1u) {
      for (auto child_node : node->nodes) {
        todo.emplace_back(child_node, next_depth);
      }
    }
  }
}

//! Install a new generator to back the data of this model.
void TreeGeneratorModel::InstallGenerator(ITreeGeneratorPtr generator_) {
  std::deque<Node> old_nodes;

  CancelRunningRequest();

  emit beginResetModel();
  d->version_number.fetch_add(1u);
  d->root.nodes.clear();
  d->root.parent = &(d->root);
  d->root.self_or_duplicate = nullptr;
  d->generator = std::move(generator_);
  d->num_columns = d->generator ? d->generator->NumColumns() : 0u;
  d->entity_to_node.clear();
  d->entity_to_node.emplace(kInvalidEntityId, &(d->root));
  d->nodes.swap(old_nodes);
  emit endResetModel();

  if (d->generator) {
    d->root.self_or_duplicate = &(d->root);
    RunExpansionThread(new InitTreeRunnable(
        d->generator, d->version_number, {},
        d->generator->InitialExpansionDepth()));
  }
}

QModelIndex TreeGeneratorModel::index(int row, int column,
                                     const QModelIndex &parent) const {
  if (!hasIndex(row, column, parent)) {
    return {};
  }

  if (column < 0 || column >= d->num_columns) {
    return {};
  }

  Node *parent_node = &(d->root);
  if (parent.isValid()) {
    parent_node = reinterpret_cast<Node *>(parent.internalPointer());
  }

  if (!parent_node) {
    return {};
  }

  auto row_index = static_cast<unsigned>(row);
  if (row_index >= parent_node->nodes.size()) {
    return {};
  }

  auto child_node = parent_node->nodes[row_index];
  Q_ASSERT(row == child_node->row);

  return createIndex(row, column, child_node);
}

QModelIndex TreeGeneratorModel::parent(const QModelIndex &child) const {
  if (!child.isValid()) {
    return {};
  }

  auto child_node = reinterpret_cast<Node *>(child.internalPointer());
  auto parent_node = child_node->parent;
  if (!parent_node || parent_node == &(d->root)) {
    return {};
  }

  return createIndex(parent_node->row, 0, parent_node);
}

int TreeGeneratorModel::rowCount(const QModelIndex &parent) const {
  if (parent.column() >= 1) {
    return 0;
  }

  Node *parent_node = &(d->root);
  if (parent.isValid()) {
    parent_node = reinterpret_cast<Node *>(parent.internalPointer());
  }

  return parent_node ? static_cast<int>(parent_node->nodes.size()) : 0;
}

int TreeGeneratorModel::columnCount(const QModelIndex &) const {
  return d->num_columns;
}

QVariant TreeGeneratorModel::headerData(int section, Qt::Orientation orientation,
                                       int role) const {

  if (orientation != Qt::Horizontal || role != Qt::DisplayRole || section < 0 ||
      section >= d->num_columns || !d->generator) {
    return QVariant();
  }

  return d->generator->ColumnTitle(section);
}

QVariant TreeGeneratorModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid()) {
    return {};
  }

  auto column = index.column();
  if (column < 0 || column >= d->num_columns) {
    return {};
  }

  Node *node = reinterpret_cast<Node *>(index.internalPointer());
  if (!node) {
    return {};
  }

  if (role == Qt::DisplayRole || role == IModel::TokenRangeDisplayRole) {
    QVariant data = node->item->Data(column);
    if (!data.isValid()) {
      return {};
    }

    if (role == Qt::DisplayRole) {
      if (auto as_str = TryConvertToString(data)) {
        return as_str.value();
      }

    } else {
      if (data.canConvert<TokenRange>() &&
          !data.value<TokenRange>().data().empty()) {
        return data;
      }
    }
  
  // Tooltip used for hovering. Also, this is used for the copy details.
  } else if (role == Qt::ToolTipRole) {
    QString tooltip
        = tr("Entity Id: %1").arg(::mx::EntityId(node->item->Entity()).Pack());
    if (d->generator) {
      for (int i = 0; i < d->num_columns; ++i) {
        QVariant col_data = node->item->Data(i);
        if (auto as_str = TryConvertToString(col_data)) {
          tooltip += QString("\n%1: %2")
                         .arg(d->generator->ColumnTitle(i))
                         .arg(as_str.value());
        }
      }
    }
    return tooltip;

  } else if (role == IModel::EntityRole) {
    auto entity = node->item->AliasedEntity();
    if (std::holds_alternative<NotAnEntity>(entity)) {
      entity = node->item->Entity();
    }

    if (!std::holds_alternative<NotAnEntity>(entity)) {
      return QVariant::fromValue(entity);
    }

  } else if (role == IModel::ModelIdRole) {
    return "com.trailofbits.model.TreeGeneratorModel";

  } else if (role == TreeGeneratorModel::CanBeExpanded) {
    return QVariant::fromValue(!node->self_or_duplicate);

  } else if (role == TreeGeneratorModel::IsDuplicate) {
    return QVariant::fromValue(
        node->self_or_duplicate && node->self_or_duplicate != node);
  }

  return {};
}

void TreeGeneratorModel::CancelRunningRequest(void) {
  if (!d->num_pending_requests) {
    return;
  }

  d->version_number.fetch_add(1u);
}

//! Notify us when there's a batch of new data to update.
void TreeGeneratorModel::AddData(
    uint64_t version_number, RawEntityId parent_node_id,
    QVector<IGeneratedItemPtr> child_items, unsigned remaining_depth) {

  if (version_number != d->version_number.load()) {
    return;
  }

  for (auto &child_item : child_items) {
    QueuedItem &entry = d->insertion_queue.emplace_back();
    entry.version_number = version_number;
    entry.parent_entity_id = parent_node_id;
    entry.item = std::move(child_item);
    entry.remaining_depth = remaining_depth;
  }

  if (!d->import_timer.isActive()) {
    d->import_timer.start(kBatchIntervalTime);
  }
}

void TreeGeneratorModel::ProcessData(void) {
  size_t num_changes = 0u;

  auto version_number = d->version_number.load();

  Node *root_node = &(d->root);

  std::unordered_map<Node *, NodePtrList> pending_inserts;
  NodePtrList ordered_pending_inserts;

  // Add a child to a parent node, and return the eventual index that the
  // child will reside in.
  auto add_child = [&] (Node *parent_node, Node *node) {
    Q_ASSERT(parent_node->self_or_duplicate == parent_node);

    auto &children = pending_inserts[parent_node];
    auto index = children.size() + parent_node->nodes.size();
    node->row = static_cast<int>(index);
    if (children.empty()) {
      ordered_pending_inserts.push_back(parent_node);
    }
    children.emplace_back(node);
  };

  while (!d->insertion_queue.empty()) {

    // If we've made too many changes overall then stop now and push things
    // to the next time.
    if (num_changes >= kMaxBatchSize) {
      break;
    }

    QueuedItem entry = std::move(d->insertion_queue.front());
    d->insertion_queue.pop_front();

    // If the version number is wrong then this is batched data for some
    // previous entity, and so we want to ignore it.
    if (entry.version_number != version_number) {
      continue;
    }

    auto eid = ::mx::EntityId(entry.item->Entity()).Pack();
    if (eid == kInvalidEntityId) {
      continue;
    }

    auto aliased_eid = ::mx::EntityId(entry.item->AliasedEntity()).Pack();

    // Build up the entity node.
    Node *entity_node = &(d->nodes.emplace_back());
    entity_node->item = std::move(entry.item);

    bool is_duplicate = false;

    auto &prev_node = d->entity_to_node[eid];
    if (prev_node) {
      entity_node->self_or_duplicate = prev_node;
      is_duplicate = true;
    } else {
      prev_node = entity_node;
    }

    if (aliased_eid != kInvalidEntityId && aliased_eid != eid) {
      auto &aliased_node = d->entity_to_node[aliased_eid];
      if (aliased_node) {
        entity_node->self_or_duplicate = aliased_node;
        is_duplicate = true;
      } else {
        aliased_node = entity_node;
      }
    }

    add_child(d->entity_to_node[entry.parent_entity_id], entity_node);
    ++num_changes;

    if (!is_duplicate && entry.remaining_depth) {
      Q_ASSERT(!entity_node->self_or_duplicate);
      Q_ASSERT(entity_node->Deduplicate() == entity_node);
      entity_node->self_or_duplicate = entity_node;
      RunExpansionThread(new ExpandTreeRunnable(
          d->generator, d->version_number, entity_node->item,
          entry.remaining_depth));
    }
  }

  // Emit the signals to mutate the tree with the updates produced by this
  // batch.
  for (auto parent_node : ordered_pending_inserts) {
    Q_ASSERT(parent_node->Deduplicate() == parent_node);
    Q_ASSERT(parent_node->parent != nullptr);

    auto children = std::move(pending_inserts[parent_node]);
    if (children.empty()) {
      continue;
    }

    QModelIndex parent_index;
    if (parent_node != root_node) {
      parent_index = createIndex(parent_node->row, 0, parent_node);
    }

    auto num_new_children = static_cast<int>(children.size());
    auto num_curr_children = static_cast<int>(parent_node->nodes.size());
      
    Q_ASSERT(0 < num_new_children);

    emit beginInsertRows(
        parent_index, num_curr_children,
        num_curr_children + num_new_children - 1);

    for (auto child_node : children) {
      Q_ASSERT(!child_node->parent);
      Q_ASSERT(static_cast<unsigned>(child_node->row) ==
               parent_node->nodes.size());
      child_node->parent = parent_node;
      parent_node->nodes.emplace_back(child_node);
    }

    emit endInsertRows();
  }

  // If there's still anything left then restart the timer to import more.
  if (!d->insertion_queue.empty()) {
    d->import_timer.start(kBatchIntervalTime);
  }
}

}  // namespace mx::gui