// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "EntityInformationModel.h"

#include <QMap>
#include <QString>
#include <QTimer>

#include <list>
#include <multiplier/GUI/Interfaces/IInfoGenerator.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/Index.h>
#include <unordered_map>
#include <vector>

#include "EntityInformationRunnable.h"

namespace mx::gui {
namespace {

static constexpr int kBatchIntervalTime = 250;

struct Node;

using NodePtr = std::unique_ptr<Node>;
using NodePtrList = std::vector<std::unique_ptr<Node>>;

struct Node {
  QString name;
  IInfoGeneratorItemPtr item;

  // Parent node.
  Node *parent{nullptr};

  // List of child nodes.
  NodePtrList nodes;

  // Maps from a node name to an index in `nodes`.
  QMap<QString, size_t> node_index;

  // Index of this node within `parent`.
  int row{0};

  // Is this node a category node (one with children), or an entity node
  // (a leaf node)?
  bool is_category{true};

  // Should the name be what is rendered for the `Qt::DisplayRole`?
  bool render_name{true};
};

static QString TokensToString(TokenRange tokens) {
  QString as_str;
  for (Token tok : tokens) {
    std::string_view data = tok.data();
    if (!data.empty()) {
      as_str += QString::fromUtf8(
          data.data(), static_cast<qsizetype>(data.size()));
    }
  }
  return as_str;
}

}  // namespace

struct EntityInformationModel::PrivateData {
  FileLocationCache file_location_cache;
  const AtomicU64Ptr version_number;
  Node root;
  QTimer import_timer;
  QMap<QString, std::list<std::pair<uint64_t, IInfoGeneratorItemPtr>>>
      insertion_queue;

  inline PrivateData(const FileLocationCache &file_location_cache_,
                     AtomicU64Ptr version_number_)
      : file_location_cache(file_location_cache_),
        version_number(std::move(version_number_)) {}
};

EntityInformationModel::~EntityInformationModel(void) {}

EntityInformationModel::EntityInformationModel(
    const FileLocationCache &file_location_cache, AtomicU64Ptr version_number,
    QObject *parent)
    : IModel(parent),
      d(new PrivateData(file_location_cache, std::move(version_number))) {

  connect(&d->import_timer, &QTimer::timeout, this,
          &EntityInformationModel::ProcessData);
}

QModelIndex EntityInformationModel::index(
    int row, int column, const QModelIndex &parent) const {
  
  if (!hasIndex(row, column, parent)) {
    return {};
  }

  if (column != 0) {
    return {};
  }

  Node *parent_node = &d->root;
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

  auto child_node = parent_node->nodes[row_index].get();
  Q_ASSERT(row == child_node->row);

  return createIndex(row, 0, child_node);
}

QModelIndex EntityInformationModel::parent(const QModelIndex &child) const {
  if (!child.isValid() || child.column() != 0) {
    return {};
  }

  auto child_node = reinterpret_cast<Node *>(child.internalPointer());
  auto parent_node = child_node->parent;
  if (!parent_node || parent_node == &(d->root)) {
    return {};
  }

  return createIndex(parent_node->row, 0, parent_node);
}

int EntityInformationModel::rowCount(const QModelIndex &parent) const {
  Node *parent_node = &d->root;
  if (parent.isValid()) {
    parent_node = reinterpret_cast<Node *>(parent.internalPointer());
  }

  return parent_node ? static_cast<int>(parent_node->nodes.size()) : 0;
}

int EntityInformationModel::columnCount(const QModelIndex &) const {
  return 1;
}

QVariant EntityInformationModel::data(
    const QModelIndex &index, int role) const {
  if (!index.isValid() || index.column()) {
    return {};
  }

  auto node = reinterpret_cast<Node *>(index.internalPointer());

  if (role == Qt::DisplayRole) {
    if (!node->render_name && node->item) {
      return TokensToString(node->item->Tokens());
    }
    return node->name;

  } else if (role == IModel::TokenRangeDisplayRole) {
    if (!node->render_name && node->item) {
      return QVariant::fromValue(node->item->Tokens());
    }
  
  } else if (role == IModel::EntityRole) {
    if (node->item && !node->is_category) {
      return QVariant::fromValue(node->item->Entity());
    }
  }

  return {};
}

void EntityInformationModel::AddData(
    uint64_t version_number, const QString &category,
    QVector<IInfoGeneratorItemPtr> items) {
  if (version_number != d->version_number->load()) {
    return;
  }

  auto &list = d->insertion_queue[category];
  for (auto &item : items) {
    list.emplace_back(version_number, std::move(item));
  }

  if (!d->import_timer.isActive()) {
    d->import_timer.start(kBatchIntervalTime);
  }
}

void EntityInformationModel::ProcessData(void) {
  size_t num_changes = 0u;

  auto version_number = d->version_number->load();

  Node *root_node = &(d->root);

  std::unordered_map<Node *, NodePtrList> pending_inserts;
  std::vector<Node *> ordered_pending_inserts;

  // Add a child to a parent node, and return the eventual index that the
  // child will reside in.
  auto add_child = [&] (Node *parent_node, Node *node) -> size_t {
    Q_ASSERT(parent_node->is_category);

    auto &children = pending_inserts[parent_node];
    auto index = children.size() + parent_node->nodes.size();
    node->row = static_cast<int>(index);
    if (children.empty()) {
      ordered_pending_inserts.push_back(parent_node);
    }
    children.emplace_back(node);
    return index;
  };

  // Get the Nth child, taking into account that we might have to take it out
  // of a pending inserts list.
  auto get_child = [&] (Node *parent_node, size_t index) -> Node * {
    Q_ASSERT(parent_node->is_category);

    auto num_children = parent_node->nodes.size();
    if (index < num_children) {
      return parent_node->nodes[index].get();
    } else {
      return pending_inserts[parent_node][index - num_children].get();
    }
  };

  for (auto iq_it = d->insertion_queue.begin();
       iq_it != d->insertion_queue.end(); ++iq_it) {
    const QString &category = iq_it.key();
    auto &pending_items = iq_it.value();

    // If we've aleady made changes, and the next set of changes is going to
    // be too big, then put them off.
    if (num_changes && (num_changes + pending_items.size()) > kMaxBatchSize) {
      break;
    }

    Node *category_node = nullptr;
    auto category_added = false;

    while (!pending_items.empty()) {

      // If we've made too many changes overall then stop now and push things
      // to the next time.
      if (num_changes >= kMaxBatchSize) {
        break;
      }

      auto item = std::move(pending_items.front());
      pending_items.pop_front();

      // If the version number is wrong then this is batched data for some
      // previous entity, and so we want to ignore it.
      if (item.first != version_number) {
        continue;
      }

      // If we don't have a category node yet (first pending item for this
      // category) then we might have to create the category. If we create a new
      // category then this ends up being the simplest from a signal management
      // perspective.
      if (!category_node) {
        if (auto it = root_node->node_index.find(category);
            it != root_node->node_index.end()) {
          category_node = root_node->nodes[it.value()].get();
        } else {
          category_node = new Node;
          category_node->name = category;
          category_node->row = static_cast<int>(root_node->nodes.size());
          category_node->parent = root_node;

          emit beginInsertRows(QModelIndex(), category_node->row,
                               category_node->row);
          
          root_node->nodes.emplace_back(category_node);
          root_node->node_index.insert(
              category, static_cast<unsigned>(category_node->row));
          category_added = true;
          ++num_changes;
        }
      }

      // Build up the entity node.
      Node *entity_node = new Node;
      entity_node->item = std::move(item.second);
      entity_node->is_category = false;
      entity_node->render_name = false;
      entity_node->name = TokensToString(entity_node->item->Tokens());

      // Look to see if another node with the same name exists within
      // `category_node`.
      Node *prev_data_node = nullptr;
      auto it = category_node->node_index.find(entity_node->name);
      if (it != category_node->node_index.end()) {
        prev_data_node = get_child(category_node, it.value());
      }

      // A sub-category doesn't exist, and a conflicting that needs to be
      // converted into a sub-category doesn't exist, so we're adding to the
      if (!prev_data_node) {

        category_node->node_index.insert(
            entity_node->name, add_child(category_node, entity_node));

      // A sub-category does exist, we need to place stuff inside of it.
      } else if (prev_data_node->is_category) {

        entity_node->name = entity_node->item->Location();
        entity_node->render_name = true;

        prev_data_node->node_index.insert(
            entity_node->name, add_child(prev_data_node, entity_node));

      // An entity with a conflicting name exists; we need to make a new
      // category and move the conflicting entity data into that new sub-
      // category.
      } else {
        auto cloned_node = new Node;
        cloned_node->name = prev_data_node->item->Location();
        cloned_node->render_name = true;
        cloned_node->item = prev_data_node->item;
        cloned_node->is_category = prev_data_node->is_category;

        entity_node->name = entity_node->item->Location();
        entity_node->render_name = true;

        prev_data_node->is_category = true;
        
        prev_data_node->node_index.insert(
            entity_node->name, add_child(prev_data_node, cloned_node));

        prev_data_node->node_index.insert(
            entity_node->name, add_child(prev_data_node, entity_node));

        prev_data_node->nodes.emplace_back(cloned_node);

        // If it was already linked into the tree.
        if (prev_data_node->parent) {

          // NOTE(pag): We `std::move(prev_data_node->item)` into the child
          //            `cloned_node`, and so we emit `dataChanged` because the
          //            `IModel::EntityRole` will now return something
          //            different, which could affect row highlighting when
          //            using the `ThemedItemDelegate` in conjunction with a
          //            `HighlightExplorer`-added theme proxy (to color-
          //            highlight stuff).
          auto node_index = createIndex(prev_data_node->row, 0, prev_data_node);
          emit dataChanged(node_index, node_index);
        }

        ++num_changes;
      }

      ++num_changes;
    }

    if (category_added) {
      for (auto parent_node : ordered_pending_inserts) {
        Q_ASSERT(parent_node->is_category);

        for (auto &child_node_ptr : pending_inserts[parent_node]) {
          child_node_ptr->parent = parent_node;
          parent_node->nodes.emplace_back(std::move(child_node_ptr));
        }
      }

      emit endInsertRows();
      continue;
    }

    for (auto parent_node : ordered_pending_inserts) {
      Q_ASSERT(parent_node->is_category);

      QModelIndex parent_index;
      if (parent_node->parent == root_node) {
        parent_index = index(parent_node->row, 0, QModelIndex());
      } else {
        parent_index = createIndex(parent_node->row, 0, parent_node);
      }

      auto &children = pending_inserts[parent_node];
      auto num_curr_children = static_cast<int>(parent_node->nodes.size());
      auto num_new_children = static_cast<int>(children.size());
      Q_ASSERT(0 < num_new_children);

      emit beginInsertRows(
          parent_index, num_curr_children,
          num_curr_children + num_new_children - 1);

      for (auto &child_node_ptr : children) {
        child_node_ptr->parent = parent_node;
        parent_node->nodes.emplace_back(std::move(child_node_ptr));
      }

      emit endInsertRows();
    }
  }

  // If there's still anything left then restart the timer to import more.
  for (auto it = d->insertion_queue.begin();
       it != d->insertion_queue.end(); ++it) {
    if (it.value().size()) {
      d->import_timer.start(kBatchIntervalTime);
      return;
    }
  }
}

void EntityInformationModel::OnIndexChanged(
    const ConfigManager &config_manager) {
  emit beginResetModel();
  d->version_number->fetch_add(1u);
  d->file_location_cache = config_manager.FileLocationCache();
  d->insertion_queue.clear();
  d->root.node_index.clear();
  d->root.nodes.clear();
  emit endResetModel();
}

}  // namespace mx::gui
