/*
  Copyright (c) 2024-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "ListGeneratorModel.h"

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
#include <list>
#include <multiplier/GUI/Interfaces/IListGenerator.h>
#include <multiplier/GUI/Util.h>
#include <unordered_map>

#include "InitTreeRunnable.h"

namespace mx::gui {
namespace {

struct Node;

using NodeKey = std::pair<const RawEntityId, Node>;

struct Node {

  // The generated item associated with this node.
  IGeneratedItemPtr item;

  int row{-1};

  // Index into `d->child_keys`. If this node isn't a duplicate, then this index
  // will reference back to the node itself. Otherwise it will reference the
  // first/original node.
  unsigned alias_index{0u};
};

struct DataBatch {
  std::list<IGeneratedItemPtr> child_items;

  inline DataBatch(QVector<IGeneratedItemPtr> child_items_) {
    for (auto &item : child_items_) {
      child_items.emplace_back(std::move(item));
    }
  }
};

using DataBatchQueue = std::list<DataBatch>;

}  // namespace

struct ListGeneratorModel::PrivateData final {

    // Data generator.
  IListGeneratorPtr generator;

  // The non-uniqued nodes of the tree.
  std::deque<NodeKey *> child_keys;

  // The redundant node keys. Sometimes we already have something in
  // `entity_to_node`, in which case, we leave it there, but we still make
  // a redundant node here in `redundant_keys` so that we can link together
  // things into a sibling list, but also know that we shouldn't actually
  // expand underneath something.
  std::deque<NodeKey> redundant_keys;

  // The uniqued nodes of the tree.
  std::unordered_map<RawEntityId, Node> entity_to_node;

  // Used to help deduplicate.
  std::unordered_map<RawEntityId, NodeKey *> aliased_entity_to_key;

  // Returns the if there's an outstanding request.
  int num_pending_requests{0};

  // Version number of this model. This is incremented when we install a new
  // generator.
  std::atomic_uint64_t version_number;

  // Thread pool for all expansion runnables.
  QThreadPool thread_pool;

  //! A timer used to import data from the data batch queue
  QTimer import_timer;

  // Queue of groups of children `IGeneratedItem`s to insert into the model.
  DataBatchQueue data_batch_queue;

  inline PrivateData(void)
      : version_number(0u) {}

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

  void ImportData(Node *new_node, IGeneratedItemPtr item);
};

//! Constructor
ListGeneratorModel::ListGeneratorModel(QObject *parent)
    : IModel(parent),
      d(new PrivateData) {
  connect(&d->import_timer, &QTimer::timeout, this,
          &ListGeneratorModel::ProcessDataBatchQueue);
}

ListGeneratorModel::~ListGeneratorModel(void) {
  CancelRunningRequest();
}

//! Find the original version of an item.
QModelIndex ListGeneratorModel::Deduplicate(const QModelIndex &index) {
  NodeKey *node_key = d->NodeKeyFrom(index);
  if (!node_key) {
    return {};
  }

  return createIndex(node_key->second.row, 0, node_key);
}

//! Install a new generator to back the data of this model.
void ListGeneratorModel::InstallGenerator(IListGeneratorPtr generator_) {

  CancelRunningRequest();

  emit beginResetModel();
  d->version_number.fetch_add(1u);
  d->generator = std::move(generator_);
  d->entity_to_node.clear();
  d->aliased_entity_to_key.clear();
  d->child_keys.clear();
  d->redundant_keys.clear();
  d->import_timer.stop();
  d->data_batch_queue.clear();
  emit endResetModel();

  // Start a request to fetch the data.
  if (d->generator) {
    d->num_pending_requests += 1;

    auto runnable = new InitTreeRunnable(
        d->generator, d->version_number, {}, 1u);

    connect(runnable, &IGenerateTreeRunnable::NewGeneratedItems,
            this, &ListGeneratorModel::OnNewListItems);

    connect(runnable, &IGenerateTreeRunnable::Finished,
            this, &ListGeneratorModel::OnRequestFinished);

    d->import_timer.start(kBatchIntervalTime);
    emit RequestStarted();

    d->thread_pool.start(runnable);
  }
}

QModelIndex ListGeneratorModel::index(int row, int column,
                                      const QModelIndex &parent) const {

  if (!hasIndex(row, column, parent)) {
    return {};
  }

  if (parent.isValid() || column != 0) {
    return {};
  }

  auto row_count = static_cast<int>(d->child_keys.size());
  if (0 > row || row >= row_count) {
    return {};
  }

  return createIndex(row, 0, d->child_keys[static_cast<unsigned>(row)]);
}

QModelIndex ListGeneratorModel::parent(const QModelIndex &) const {
  return QModelIndex();
}

int ListGeneratorModel::rowCount(const QModelIndex &parent) const {
  return !parent.isValid() ? static_cast<int>(d->child_keys.size()) : 0;
}

int ListGeneratorModel::columnCount(const QModelIndex &) const {
  return d->generator ? 1 : 0;
}

QVariant ListGeneratorModel::headerData(int section, Qt::Orientation orientation,
                                       int role) const {

  if (orientation != Qt::Horizontal || role != Qt::DisplayRole ||
      section != 0 || !d->generator) {
    return QVariant();
  }

  return d->generator->ColumnTitle(section);
}

QVariant ListGeneratorModel::data(const QModelIndex &index, int role) const {
  QVariant value;
  if (index.column() != 0) {
    return value;
  }

  NodeKey *entity_key = d->NodeKeyFrom(index);
  if (!entity_key) {
    return value;
  }

  Node *node = &(entity_key->second);

    QVariant data = node->item->Data(index.column());
  if (!data.isValid()) {
    return data;
  }

  if (role == Qt::DisplayRole) {
    if (auto as_str = TryConvertToString(data)) {
      return as_str.value();
    }

  } else if (role == IModel::TokenRangeDisplayRole) {
    if (data.canConvert<TokenRange>() &&
        !data.value<TokenRange>().data().empty()) {
      return data;
    }
  
  // Tooltip used for hovering. Also, this is used for the copy details.
  } else if (role == Qt::ToolTipRole) {
    QString tooltip = tr("Entity Id: ") + QString::number(entity_key->first);
    if (d->generator) {
      QVariant col_data = node->item->Data(0);
      if (auto as_str = TryConvertToString(col_data)) {
        tooltip += QString("\n%1: %2").arg(d->generator->ColumnTitle(0)).arg(as_str.value());
      }
    }
    value.setValue(tooltip);

  } else if (role == IModel::EntityRole) {
    auto entity = node->item->AliasedEntity();
    if (std::holds_alternative<NotAnEntity>(entity)) {
      entity = node->item->Entity();
    }

    if (!std::holds_alternative<NotAnEntity>(entity)) {
      value.setValue(entity);
    }

  } else if (role == IModel::ModelIdRole) {
    return "com.trailofbits.model.ListGeneratorModel";

  } else if (role == ListGeneratorModel::IsDuplicate) {
    value.setValue(static_cast<int>(node->alias_index) != node->row);
  }

  return value;
}

void ListGeneratorModel::OnRequestFinished(void) {
  d->num_pending_requests -= 1;
  Q_ASSERT(d->num_pending_requests >= 0);
  if (!d->num_pending_requests) {
    emit RequestFinished();
  }
}

void ListGeneratorModel::CancelRunningRequest(void) {
  if (!d->num_pending_requests && d->data_batch_queue.empty()) {
    return;
  }

  d->version_number.fetch_add(1u);
  d->import_timer.stop();
  d->data_batch_queue.clear();
}

//! Notify us when there's a batch of new data to update.
void ListGeneratorModel::OnNewListItems(
    uint64_t version_number, RawEntityId,
    QVector<IGeneratedItemPtr> child_items, unsigned) {

  if (version_number != d->version_number.load()) {
    return;
  }

  d->data_batch_queue.emplace_back(std::move(child_items));
}

void ListGeneratorModel::ProcessDataBatchQueue(void) {

  // Count how many items we've imported so that we can batch them across
  // timer events.
  int num_imported = 0;

  while (!d->data_batch_queue.empty()) {
    DataBatch &batch = d->data_batch_queue.front();
    if (batch.child_items.empty()) {
      d->data_batch_queue.pop_front();
      continue;
    }

    int num_imported_children = 0;
    const int prev_num_children = static_cast<int>(d->child_keys.size());

    while (!batch.child_items.empty()) {

      auto item = std::move(batch.child_items.front());
      batch.child_items.pop_front();

      VariantEntity entity = item->Entity();
      if (std::holds_alternative<NotAnEntity>(entity)) {
        continue;
      }

      auto eid = ::mx::EntityId(entity).Pack();
      if (eid == kInvalidEntityId) {
        continue;
      }

      auto aliased_entity = item->AliasedEntity();
      if (std::holds_alternative<NotAnEntity>(aliased_entity)) {
        aliased_entity = entity;
      }

      // Now create the node key. If this is the first time we're seeing the
      // node, then the node key is in our `entity_to_node` map; otherwise we
      // make a redundant key in `redundant_keys`.
      NodeKey *curr_key = nullptr;
      auto [node_it, added] = d->entity_to_node.emplace(eid, Node{});
      NodeKey *load_key = &*node_it;
      if (added) {
        curr_key = load_key;

        // Even though this is a new node, link it to a prior node. This is to
        // allow us to reprepresent another form of equivalence to the
        // deduplication mechanism, i.e. that one declaration may be a
        // redeclaration of another one.
        const RawEntityId aliased_eid = ::mx::EntityId(aliased_entity).Pack();
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
        curr_key = &(d->redundant_keys.emplace_back(eid, Node{}));
      }

      Node *const new_node = &(curr_key->second);

      // Copy the entity into the node.
      new_node->item = std::move(item);

      // Make the node point to itself, and update the parent child index or
      // previous sibling's next sibling index.
      new_node->alias_index = static_cast<unsigned>(d->child_keys.size());
      new_node->row = static_cast<int>(new_node->alias_index);

      // Possibly make the node point to its alias. Note that `load_key` may
      // not be `new_node`.
      new_node->alias_index = load_key->second.alias_index;

      d->child_keys.emplace_back(curr_key);

      ++num_imported_children;
      ++num_imported;

      if (kMaxBatchSize <= num_imported) {
        break;
      }
    }

    // We didn't end up importing anything.
    if (!num_imported_children) {
      d->data_batch_queue.pop_front();
      continue;
    }

    // Update the number of children of the parent.
    emit beginInsertRows(
        QModelIndex(), prev_num_children,
        prev_num_children + num_imported_children - 1);

    emit endInsertRows();

    if (batch.child_items.empty()) {
      d->data_batch_queue.pop_front();
    }

    if (kMaxBatchSize <= num_imported) {
      break;
    }
  }

  bool has_remaining = !d->data_batch_queue.empty() ||
                       0 < d->num_pending_requests;

  // Restart the timer, so that the import procedure will fire again
  // in kBatchIntervalTime msecs from the end of the previous batch.
  if (has_remaining) {
    if (!d->import_timer.isActive()) {
      d->import_timer.start(kBatchIntervalTime);
    }

  } else {
    d->import_timer.stop();
  }
}

}  // namespace mx::gui