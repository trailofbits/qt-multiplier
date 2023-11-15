/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "TreeExplorerModel.h"
#include "ITreeExplorerExpansionThread.h"
#include "InitTreeExplorerThread.h"
#include "ExpandTreeExplorerThread.h"

#include <QFutureWatcher>
#include <QThreadPool>
#include <QTimer>
#include <QtConcurrent>
#include <QDebug>

namespace mx::gui {

namespace {

//! The ID of the internal root item
const TreeExplorerModel::Context::Node::ID kRootNodeID{1};

//! Update timer used for first requests
const int kFirstUpdateInterval{500};

//! Standard update timer
const int kImportInterval{1500};

//! Batch size
const int kMaxBatchSize{100};

}  // namespace

struct TreeExplorerModel::PrivateData final {
  //! The tree data
  Context context;

  //! The active data generator.
  std::shared_ptr<ITreeGenerator> generator;

  //! Column count, as returned by the generator
  std::size_t column_count{};

  //! Version number of this model. This is incremented when we install a new
  //! generator.
  std::atomic_uint64_t version_number{};

  //! Future used to resolve the name of the tree.
  QFuture<QString> tree_name_future;

  //! Notifies us when the tree name has been fetched
  QFutureWatcher<QString> tree_name_future_watcher;

  //! A timer used to import the data received from the `generator`
  QTimer import_timer;

  //! Active expansion threads
  std::vector<ITreeExplorerExpansionThread *> expansion_thread_list;

  //! Local thread pool, used for the expansion threads
  QThreadPool thread_pool;
};

TreeExplorerModel::TreeExplorerModel(QObject *parent)
    : ITreeExplorerModel(parent),
      d(new PrivateData) {

  connect(&(d->tree_name_future_watcher),
          &QFutureWatcher<QFuture<QString>>::finished, this,
          &TreeExplorerModel::OnNameResolved);

  connect(&d->import_timer, &QTimer::timeout, this,
          &TreeExplorerModel::ProcessDataBatchQueue);

  d->import_timer.start(kImportInterval);
}

TreeExplorerModel::~TreeExplorerModel() {
  CancelRunningRequest();
}

void TreeExplorerModel::RunExpansionThread(
    ITreeExplorerExpansionThread *expander) {

  connect(expander, &ITreeExplorerExpansionThread::NewTreeItems, this,
          &TreeExplorerModel::OnNewTreeItems);


  auto is_first_request = (d->thread_pool.activeThreadCount() == 0);
  emit RequestStarted();

  d->thread_pool.start(expander);

  // Hasten the first import
  if (is_first_request) {
    d->import_timer.start(kFirstUpdateInterval);
  }
}

void TreeExplorerModel::InstallGenerator(
    std::shared_ptr<ITreeGenerator> generator_) {

  CancelRunningRequest();

  emit beginResetModel();

  d->generator = std::move(generator_);
  d->column_count = static_cast<std::size_t>(d->generator->NumColumns());
  d->version_number.fetch_add(1u);

  InitializeContext(d->context);

  emit endResetModel();

  // Start a request to fetch the name of this tree.
  d->tree_name_future = QtConcurrent::run(
      [gen = d->generator](void) -> QString { return gen->TreeName(gen); });

  d->tree_name_future_watcher.setFuture(d->tree_name_future);

  // Mark the internal root item as "opening", then start the
  // expansion task
  auto root_node_ptr = GetNodeFromID(d->context, kRootNodeID);
  if (root_node_ptr == nullptr) {
    qDebug() << "The internal root node is missing";
    return;
  }

  auto &root_node = *root_node_ptr;
  root_node.state = Context::Node::State::Opening;

  RunExpansionThread(new InitTreeExplorerThread(d->generator, d->version_number,
                                                kInvalidEntityId, 2u));
}

void TreeExplorerModel::Expand(const QModelIndex &index,
                               const std::size_t &depth) {

  // Skip if this is the root item or if we don't need to do
  // any expansion
  if (depth == 0 || !index.isValid()) {
    return;
  }

  // Check the node state before we start a new expansion request
  auto node_id = static_cast<Context::Node::ID>(index.internalId());

  auto node_ptr = GetNodeFromID(d->context, node_id);
  if (node_ptr == nullptr) {
    return;
  }

  auto &node = *node_ptr;
  if (node.state != Context::Node::State::Unopened) {
    return;
  }

  // Start the expansion request
  node.state = Context::Node::State::Opening;

  RunExpansionThread(new ExpandTreeExplorerThread(
      d->generator, d->version_number, node.data.entity_id,
      static_cast<unsigned>(depth)));
}

QModelIndex TreeExplorerModel::Deduplicate(const QModelIndex &index) {
  // Get the node ID, and ignore the root item (both the qt one
  // and our internal one)
  if (!index.isValid()) {
    return QModelIndex();
  }

  auto node_id = static_cast<Context::Node::ID>(index.internalId());
  if (node_id == kRootNodeID) {
    return QModelIndex();
  }

  // Get the node
  auto node_ptr = GetNodeFromID(d->context, node_id);
  if (node_ptr == nullptr) {
    return QModelIndex();
  }

  const auto &node = *node_ptr;

  // If this node is aliasing another one, then return the destination
  // index
  if (!node.opt_aliased_node_id.has_value()) {
    return QModelIndex();
  }

  const auto &aliased_node_id = node.opt_aliased_node_id.value();
  return GetModelIndexFromNodeID(d->context, aliased_node_id, 0);
}

QModelIndex TreeExplorerModel::index(int row, int column,
                                     const QModelIndex &parent) const {
  if (!hasIndex(row, column, parent)) {
    return QModelIndex();
  }

  // Get the Node ID for the parent. If the `parent` model index is not
  // valid, then we'll use our internal root item
  Context::Node::ID parent_node_id{kRootNodeID};
  if (parent.isValid()) {
    parent_node_id = static_cast<Context::Node::ID>(parent.internalId());
  }

  // Attempt to get the parent node
  auto parent_node_ptr = GetNodeFromID(d->context, parent_node_id);
  if (parent_node_ptr == nullptr) {
    return QModelIndex();
  }

  const auto &parent_node = *parent_node_ptr;

  // Now look for the Nth item in the child_node_id_list vector
  auto child_node_id_list =
      std::next(parent_node.child_node_id_list.begin(), row);

  if (child_node_id_list == parent_node.child_node_id_list.end()) {
    return QModelIndex();
  }

  const auto &child_node_id = *child_node_id_list;
  return GetModelIndexFromNodeID(d->context, child_node_id, column);
}

QModelIndex TreeExplorerModel::parent(const QModelIndex &child) const {
  // Return an empty QModelIndex if `child` is already the Qt root
  if (!child.isValid()) {
    return QModelIndex();
  }

  // Attempt to get the node; node ID 0 is reserved for our internal
  // root item
  auto node_id = static_cast<Context::Node::ID>(child.internalId());
  if (node_id == 0) {
    return QModelIndex();
  }

  auto node_ptr = GetNodeFromID(d->context, node_id);
  if (node_ptr == nullptr) {
    return QModelIndex();
  }

  const auto &node = *node_ptr;

  // Create a new QModelIndex for the parent
  if (node.parent_node_id == kRootNodeID) {
    return QModelIndex();
  }

  return GetModelIndexFromNodeID(d->context, node.parent_node_id, 0);
}

int TreeExplorerModel::rowCount(const QModelIndex &parent) const {
  // Return no rows if the column is incorrect
  if (parent.column() > 0) {
    return 0;
  }

  // Get the parent node id
  Context::Node::ID parent_node_id{kRootNodeID};
  if (parent.isValid()) {
    parent_node_id = static_cast<Context::Node::ID>(parent.internalId());
  }

  // Get the parent node
  auto parent_node_ptr = GetNodeFromID(d->context, parent_node_id);
  if (parent_node_ptr == nullptr) {
    return 0;
  }

  const auto &parent_node = *parent_node_ptr;

  // Return the child count
  return static_cast<int>(parent_node.child_node_id_list.size());
}

int TreeExplorerModel::columnCount(const QModelIndex &) const {
  return static_cast<int>(d->column_count);
}

QVariant TreeExplorerModel::headerData(int section, Qt::Orientation orientation,
                                       int role) const {

  if (orientation != Qt::Horizontal || role != Qt::DisplayRole || section < 0 ||
      section >= static_cast<int>(d->column_count)) {
    return QVariant();
  }

  return d->generator->ColumnTitle(section);
}

QVariant TreeExplorerModel::data(const QModelIndex &index, int role) const {
  // The Qt root has no data
  if (!index.isValid()) {
    return QVariant();
  }

  // Validate the column number
  if (index.column() >= static_cast<int>(d->column_count)) {
    return QVariant();
  }

  // Attempt to get the node id
  auto node_id = static_cast<Context::Node::ID>(index.internalId());
  if (node_id == kRootNodeID) {
    return QVariant();
  }

  // Attempt to get the node
  auto node_ptr = GetNodeFromID(d->context, node_id);
  if (node_ptr == nullptr) {
    return QVariant();
  }

  const auto &node = *node_ptr;

  // Locate the required column
  auto column_value_list_it =
      std::next(node.data.column_value_list.begin(), index.column());

  if (column_value_list_it == node.data.column_value_list.end()) {
    return QVariant();
  }

  const auto &column_value = *column_value_list_it;

  // Return the requested data role
  QVariant value;

  switch (role) {
    case Qt::DisplayRole: {
      if (column_value.canConvert<QString>()) {
        value = column_value;

      } else if (column_value.canConvert<TokenRange>()) {
        const auto &token_range = column_value.value<TokenRange>();

        const auto &token_range_string_view = token_range.data();
        auto token_range_string_view_size =
            static_cast<qsizetype>(token_range_string_view.size());

        auto as_string_value = QString::fromUtf8(token_range_string_view.data(),
                                                 token_range_string_view_size);
        value.setValue(as_string_value);
      }

      break;
    }

    case Qt::ToolTipRole: {
      auto tooltip =
          tr("Entity id: ") + QString::number(node.data.entity_id) + "\n";

      for (std::size_t i{}; i < d->column_count; ++i) {
        QString column_name;
        if (auto column_name_var = headerData(static_cast<int>(i),
                                              Qt::Horizontal, Qt::DisplayRole);
            column_name_var.canConvert<QString>()) {
          column_name = column_name_var.toString();
        }

        if (column_name.isEmpty()) {
          column_name = tr("Column #") + QString::number(i);
        }

        QString column_display_role;
        if (const auto &column_value_var = node.data.column_value_list[i];
            column_value_var.canConvert<QString>()) {
          column_display_role = column_value_var.toString();
        }

        if (column_display_role.isEmpty()) {
          column_display_role = tr("<null>");
        }

        tooltip += column_name + ": " + column_display_role + "\n";
      }

      value.setValue(tooltip);
      break;
    }

    case ITreeExplorerModel::EntityIdRole:
      value.setValue(node.data.entity_id);
      break;

    case ITreeExplorerModel::TokenRangeRole: {
      if (column_value.canConvert<TokenRange>()) {
        value.setValue(column_value);
      }

      break;
    }

    case ITreeExplorerModel::CanBeExpanded: {
      auto can_be_expanded = node.state == Context::Node::State::Unopened;
      value.setValue(can_be_expanded);

      break;
    }

    case ITreeExplorerModel::IsDuplicate: {
      auto is_duplicate = node.opt_aliased_node_id.has_value();
      value.setValue(is_duplicate);

      break;
    }

    default: break;
  }

  return value;
}

void TreeExplorerModel::OnNameResolved(void) {
  if (d->tree_name_future.isCanceled()) {
    return;
  }

  auto tree_name = d->tree_name_future.takeResult();
  d->tree_name_future = {};

  emit TreeNameChanged(tree_name);
}

void TreeExplorerModel::CancelRunningRequest() {
  if (d->tree_name_future.isRunning()) {
    d->tree_name_future.cancel();
    d->tree_name_future_watcher.waitForFinished();
    d->tree_name_future = {};
  }

  d->version_number.fetch_add(1u);
  d->thread_pool.waitForDone();

  d->import_timer.stop();
  d->context.incoming_subtree_list.clear();

  emit RequestFinished();
}

void TreeExplorerModel::OnNewTreeItems(
    uint64_t version_number, RawEntityId parent_entity_id,
    QList<std::shared_ptr<ITreeItem>> child_items, unsigned remaining_depth) {

  // Ensure this is coming from our current generator
  if (version_number != d->version_number.load()) {
    return;
  }

  // Determine where we have to insert our new nodes; if we get
  // the magic number kInvalidEntityId, then insert at the
  // internal tree root
  Context::Node::ID parent_node_id{kRootNodeID};

  if (parent_entity_id != kInvalidEntityId) {
    auto opt_parent_node_id =
        GetNodeIDFromMxEntityID(d->context, parent_entity_id);

    if (!opt_parent_node_id.has_value()) {
      return;
    }

    parent_node_id = opt_parent_node_id.value();
  }

  // Save this data into the incoming subtree list
  Context::Subtree subtree;
  subtree.parent_node_id = parent_node_id;
  subtree.remaining_depth = static_cast<std::size_t>(remaining_depth);

  subtree.tree_item_list.insert(subtree.tree_item_list.end(),
                                std::make_move_iterator(child_items.begin()),
                                std::make_move_iterator(child_items.end()));

  d->context.incoming_subtree_list.push_back(std::move(subtree));
}

void TreeExplorerModel::ProcessDataBatchQueue() {
  ImportIncomingSubtreeList(d->context, kMaxBatchSize);
}

void TreeExplorerModel::InitializeContext(Context &context) {
  // clang-format off
  static const Context::Node kRootTreeNode{
    // Node::node_id
    kRootNodeID,
    
    // Node::parent_node_id
    kRootNodeID,

    // Node::child_node_id_list
    {},

    // Node::row
    0,

    // Node::opt_aliased_node_id
    std::nullopt,

    // Node::state
    Context::Node::State::Unopened,

    // Node::data
    {
      // Node::Data::entity_name
      { tr("ROOT") },

      // Node::Data::entity_id
      kInvalidEntityId,
    }
  };
  // clang-format on

  context = {};
  context.tree.insert({kRootNodeID, kRootTreeNode});
}

TreeExplorerModel::Context::Node::ID
TreeExplorerModel::GenerateNodeID(TreeExplorerModel::Context &context) {
  return ++context.node_id_generator;
}

TreeExplorerModel::Context::Node *TreeExplorerModel::GetNodeFromID(
    TreeExplorerModel::Context &context,
    const TreeExplorerModel::Context::Node::ID &node_id) {

  auto tree_it = context.tree.find(node_id);
  if (tree_it == context.tree.end()) {
    return nullptr;
  }

  auto &node = tree_it->second;
  return &node;
}

std::optional<TreeExplorerModel::Context::Node::ID>
TreeExplorerModel::GetNodeIDFromMxEntityID(
    const TreeExplorerModel::Context &context, const RawEntityId &entity_id) {

  auto mx_entity_id_to_node_id_it =
      context.mx_entity_id_to_node_id.find(entity_id);

  if (mx_entity_id_to_node_id_it == context.mx_entity_id_to_node_id.end()) {
    return std::nullopt;
  }

  return mx_entity_id_to_node_id_it->second;
}

QModelIndex TreeExplorerModel::GetModelIndexFromNodeID(
    TreeExplorerModel::Context &context,
    const TreeExplorerModel::Context::Node::ID &node_id,
    const int &column) const {

  auto node_ptr = GetNodeFromID(context, node_id);
  if (node_ptr == nullptr) {
    return QModelIndex();
  }

  const auto &node = *node_ptr;
  return createIndex(node.row, column,
                     static_cast<unsigned long long int>(node_id));
}

void TreeExplorerModel::ImportIncomingSubtreeList(
    Context &context, const std::size_t &max_subtree_count) {

  // Slice the exact amount of subtrees that we need to import
  Context::SubtreeList incoming_subtree_list;

  if (max_subtree_count >= context.incoming_subtree_list.size()) {
    incoming_subtree_list = std::move(context.incoming_subtree_list);
    context.incoming_subtree_list.clear();

  } else {
    auto start_rev_it = context.incoming_subtree_list.rbegin();
    auto end_rev_it =
        std::next(start_rev_it, static_cast<int>(max_subtree_count));

    incoming_subtree_list.insert(incoming_subtree_list.end(), start_rev_it,
                                 end_rev_it);

    std::advance(start_rev_it, 1);
    std::advance(end_rev_it, 1);

    context.incoming_subtree_list.erase(start_rev_it.base(), end_rev_it.base());
  }

  // Go through each one of the subtrees that we need to import
  for (auto &subtree : incoming_subtree_list) {
    // Get the parent node
    auto parent_node_ptr = GetNodeFromID(context, subtree.parent_node_id);
    if (parent_node_ptr == nullptr) {
      continue;
    }

    auto &parent_node = *parent_node_ptr;

    // Ignore this subtree if we are already expanding it
    if (parent_node.state != Context::Node::State::Opening) {
      continue;
    }

    // This incoming parent node may be empty; if that is the case, there's
    // not much else we need to do other than just updating its state
    if (subtree.tree_item_list.empty()) {
      parent_node.state = Context::Node::State::Opened;
      continue;
    }

    // Clean up the item list
    //! \todo Actually fix the data publisher and prevent it from emitting
    //        invalid items. We have to do this work here because the model
    //        verifier expects beginInsert/endInsert blocks as a transaction
    for (auto tree_item_list_it = subtree.tree_item_list.begin();
         tree_item_list_it != subtree.tree_item_list.end();) {

      auto &tree_item = *tree_item_list_it;

      if (tree_item->EntityId() == kInvalidEntityId) {
        tree_item_list_it = subtree.tree_item_list.erase(tree_item_list_it);
      } else {
        ++tree_item_list_it;
      }
    }

    // Get the model index for the parent node
    QModelIndex parent_node_index;
    if (parent_node.node_id != kRootNodeID) {
      parent_node_index =
          GetModelIndexFromNodeID(context, parent_node.node_id, 0);

      if (!parent_node_index.isValid()) {
        continue;
      }
    }

    // Start publishing the tree changes before we change any of the nodes
    auto first_added_row =
        static_cast<int>(parent_node.child_node_id_list.size());

    auto last_added_row = static_cast<int>(subtree.tree_item_list.size() - 1);

    emit beginInsertRows(parent_node_index, first_added_row, last_added_row);

    // Go through all the child nodes in the subtree, and actually import
    // them into our model
    std::vector<Context::Node::ID> processed_node_id_list;

    for (auto &tree_item : subtree.tree_item_list) {
      // Create a new node for this ITreeItem
      auto node_id = GenerateNodeID(context);
      processed_node_id_list.push_back(node_id);

      Context::Node node;
      node.node_id = node_id;
      node.parent_node_id = parent_node.node_id;
      node.row = static_cast<int>(parent_node.child_node_id_list.size());
      node.data.entity_id = tree_item->EntityId();
      node.state = Context::Node::State::Unopened;

      // Update the entity id -> node id index; if we already have a
      // a mapping, then we should mark this node as an alias for the
      // existing one
      auto insert_status = context.mx_entity_id_to_node_id.insert(
          {node.data.entity_id, node.node_id});

      const auto &is_duplicate = !insert_status.second;
      if (is_duplicate) {
        node.opt_aliased_node_id =
            context.mx_entity_id_to_node_id[node.data.entity_id];

        // Ensure we can't expand this further
        node.state = Context::Node::State::Opened;
      }

      // Save all the generated columns
      for (std::size_t i{}; i < d->column_count; ++i) {
        auto column_value = tree_item->Data(static_cast<int>(i));
        node.data.column_value_list.push_back(std::move(column_value));
      }

      // Insert the new node into our tree, then update the parent
      context.tree.insert({node_id, std::move(node)});
      parent_node.child_node_id_list.push_back(node_id);
    }

    // Close the row insert transaction
    emit endInsertRows();

    // We may still have additional expansion tasks to queue
    if (subtree.remaining_depth > 0) {
      // Go through all the nodes we processed
      for (const auto &processed_node_id : processed_node_id_list) {
        // Check the node state
        auto node_ptr = GetNodeFromID(context, processed_node_id);
        if (node_ptr == nullptr) {
          continue;
        }

        auto &node = *node_ptr;
        if (node.state != Context::Node::State::Unopened) {
          continue;
        }

        // Ignore if there is no valid entity id we can use
        if (node.data.entity_id == kInvalidEntityId) {
          continue;
        }

        // Start a new expansion thread for this node
        node.state = Context::Node::State::Opening;

        RunExpansionThread(new ExpandTreeExplorerThread(
            d->generator, d->version_number, node.data.entity_id,
            static_cast<unsigned>(subtree.remaining_depth)));
      }
    }

    processed_node_id_list.clear();
  }

  // Restart the timer, ensuring we wait kImportInterval msecs from
  // the end of the last data import
  d->import_timer.start(kImportInterval);

  // Emit the RequestFinished signal, if applicable
  if (d->thread_pool.activeThreadCount() == 0 &&
      context.incoming_subtree_list.empty()) {
    emit RequestFinished();
  }
}

}  // namespace mx::gui
