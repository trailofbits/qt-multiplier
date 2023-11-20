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

#include <unordered_set>

namespace mx::gui {

namespace {

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

const TreeExplorerModel::Context::Node::ID
    TreeExplorerModel::Context::Node::kRootNodeID{1};

TreeExplorerModel::TreeExplorerModel(QObject *parent)
    : ITreeExplorerModel(parent),
      d(new PrivateData) {

  InitializeModel();
}

TreeExplorerModel::TreeExplorerModel(const TreeExplorerModel &source_model,
                                     const QModelIndex &root_item,
                                     QObject *parent)
    : ITreeExplorerModel(parent),
      d(new PrivateData) {

  InitializeModel();

  if (!root_item.isValid()) {
    return;
  }

  auto root_node_id = static_cast<Context::Node::ID>(root_item.internalId());

  const auto &source_context = source_model.d->context;
  if (source_context.tree.count(root_node_id) == 0) {
    return;
  }

  Context new_context;
  if (ImportContextSubtree(new_context, source_context, root_node_id)) {
    d->context = std::move(new_context);

    d->generator = source_model.d->generator;
    d->version_number = source_model.d->version_number.load();

  } else {
    InstallGenerator(d->generator);
  }
}

void TreeExplorerModel::InitializeModel() {
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
  d->version_number.fetch_add(1u);

  auto column_count = static_cast<std::size_t>(d->generator->NumColumns());

  std::vector<QString> column_title_list;
  for (auto i = 0; i < column_count; ++i) {
    auto column_title = d->generator->ColumnTitle(i);
    column_title_list.push_back(std::move(column_title));
  }

  InitializeContext(d->context, std::move(column_title_list));

  emit endResetModel();

  // Start a request to fetch the name of this tree.
  d->tree_name_future = QtConcurrent::run(
      [gen = d->generator](void) -> QString { return gen->TreeName(gen); });

  d->tree_name_future_watcher.setFuture(d->tree_name_future);

  // Mark the internal root item as "opening", then start the
  // expansion task
  auto root_node_ptr = GetNodeFromID(d->context, Context::Node::kRootNodeID);
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

  // Get the node
  auto node_id = static_cast<Context::Node::ID>(index.internalId());

  auto opt_dereferenced_node_id = DereferenceNodeID(d->context, node_id);
  if (!opt_dereferenced_node_id.has_value()) {
    // This is a deduplicated node, but we can't access it yet
    return;
  }

  node_id = opt_dereferenced_node_id.value();

  auto node_ptr = GetNodeFromID(d->context, node_id);
  if (node_ptr == nullptr) {
    return;
  }

  auto &node = *node_ptr;

  // Handle the expansion request, depending on its state:
  // Unopened: Process the expansion directly
  // Opened: Forward to the child nodes
  // Opening: Skip, someone else is already handling it
  switch (node.state) {
    case Context::Node::State::Unopened:
      node.state = Context::Node::State::Opening;

      RunExpansionThread(new ExpandTreeExplorerThread(
          d->generator, d->version_number, node.data.entity_id,
          static_cast<unsigned>(depth)));

      break;

    case Context::Node::State::Opened: {
      auto next_depth = depth - 1;

      for (auto child_node_id : node.child_node_id_list) {
        opt_dereferenced_node_id = DereferenceNodeID(d->context, child_node_id);
        if (!opt_dereferenced_node_id.has_value()) {
          // This is a deduplicated node, but we can't access it yet
          continue;
        }

        child_node_id = opt_dereferenced_node_id.value();

        auto child_node_index =
            GetModelIndexFromNodeID(d->context, child_node_id, 0);

        Expand(child_node_index, next_depth);
      }

      break;
    }

    case Context::Node::State::Opening: {
      break;
    }
  }
}

QModelIndex TreeExplorerModel::Deduplicate(const QModelIndex &index) {
  // Get the node ID, and ignore the root item (both the qt one
  // and our internal one)
  if (!index.isValid()) {
    return QModelIndex();
  }

  auto node_id = static_cast<Context::Node::ID>(index.internalId());
  if (node_id == Context::Node::kRootNodeID) {
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
  if (!node.opt_aliased_entity_id.has_value()) {
    return QModelIndex();
  }

  const auto &aliased_entity_id = node.opt_aliased_entity_id.value();

  auto opt_aliased_node_id =
      GetNodeIDFromEntityID(d->context, aliased_entity_id);

  if (!opt_aliased_node_id.has_value()) {
    return QModelIndex();
  }

  const auto &aliased_node_id = opt_aliased_node_id.value();
  return GetModelIndexFromNodeID(d->context, aliased_node_id, 0);
}

QModelIndex TreeExplorerModel::index(int row, int column,
                                     const QModelIndex &parent) const {
  if (!hasIndex(row, column, parent)) {
    return QModelIndex();
  }

  // Get the Node ID for the parent. If the `parent` model index is not
  // valid, then we'll use our internal root item
  Context::Node::ID parent_node_id{Context::Node::kRootNodeID};
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
  if (node.parent_node_id == Context::Node::kRootNodeID) {
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
  Context::Node::ID parent_node_id{Context::Node::kRootNodeID};
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
  return static_cast<int>(d->context.column_title_list.size());
}

QVariant TreeExplorerModel::headerData(int section, Qt::Orientation orientation,
                                       int role) const {

  if (orientation != Qt::Horizontal || role != Qt::DisplayRole || section < 0) {
    return QVariant();
  }

  QVariant column_title;

  auto column_title_list_it =
      std::next(d->context.column_title_list.begin(), section);
  if (column_title_list_it != d->context.column_title_list.end()) {
    column_title = *column_title_list_it;
  }

  return column_title;
}

QVariant TreeExplorerModel::data(const QModelIndex &index, int role) const {
  // We don't really care what's `index` pointing to for this role
  if (role == ITreeExplorerModel::TreeNameRole) {
    return d->context.tree_name;
  }

  // The Qt root has no data
  if (!index.isValid()) {
    return QVariant();
  }

  // Validate the column number
  if (index.column() >= static_cast<int>(d->context.column_title_list.size())) {
    return QVariant();
  }

  // Attempt to get the node id
  auto node_id = static_cast<Context::Node::ID>(index.internalId());
  if (node_id == Context::Node::kRootNodeID) {
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

      for (std::size_t i{}; i < d->context.column_title_list.size(); ++i) {
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
      auto is_duplicate = node.opt_aliased_entity_id.has_value();
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

  d->context.tree_name = d->tree_name_future.takeResult();
  d->tree_name_future = {};

  emit TreeNameChanged();
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
  Context::Node::ID parent_node_id{Context::Node::kRootNodeID};

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

void TreeExplorerModel::InitializeContext(
    Context &context, const std::vector<QString> &column_title_list) {
  // clang-format off
  static const Context::Node kRootTreeNode{
    // Node::node_id
    Context::Node::kRootNodeID,
    
    // Node::parent_node_id
    Context::Node::kRootNodeID,

    // Node::child_node_id_list
    {},

    // Node::row
    0,

    // Node::opt_aliased_entity_id
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
  context.column_title_list = column_title_list;
  context.tree.insert({Context::Node::kRootNodeID, kRootTreeNode});
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
    Context &context, const std::optional<std::size_t> &opt_max_subtree_count) {

  // Slice the exact amount of subtrees that we need to import
  if (context.incoming_subtree_list.empty()) {
    return;
  }

  auto max_subtree_count =
      opt_max_subtree_count.value_or(context.incoming_subtree_list.size());

  Context::SubtreeList incoming_subtree_list;
  if (max_subtree_count >= context.incoming_subtree_list.size()) {
    incoming_subtree_list = std::move(context.incoming_subtree_list);
    context.incoming_subtree_list.clear();

  } else {
    auto subtree_count = static_cast<int>(context.incoming_subtree_list.size() -
                                          max_subtree_count);

    auto start_it =
        std::next(context.incoming_subtree_list.begin(), subtree_count);

    auto end_it = context.incoming_subtree_list.end();

    incoming_subtree_list.insert(incoming_subtree_list.end(),
                                 std::make_move_iterator(start_it),
                                 std::make_move_iterator(end_it));

    context.incoming_subtree_list.erase(start_it, end_it);
  }

  // Go through each one of the subtrees that we need to import
  for (auto subtree_it = incoming_subtree_list.rbegin();
       subtree_it != incoming_subtree_list.rend(); ++subtree_it) {

    auto &subtree = *subtree_it;

    // Get the parent node
    auto parent_node_ptr = GetNodeFromID(context, subtree.parent_node_id);
    if (parent_node_ptr == nullptr) {
      continue;
    }

    auto &parent_node = *parent_node_ptr;

    // Ignore this subtree if we are already expanding it or if we know
    // that it is a duplicate
    if (parent_node.state != Context::Node::State::Opening ||
        parent_node.opt_aliased_entity_id.has_value()) {
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
    if (parent_node.node_id != Context::Node::kRootNodeID) {
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

      Context::Node node{};
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

      auto is_duplicate = !insert_status.second;
      if (is_duplicate) {
        node.opt_aliased_entity_id = node.data.entity_id;

      } else {
        auto aliased_entity_id = tree_item->AliasedEntityId();

        if (aliased_entity_id != kInvalidEntityId &&
            aliased_entity_id != node.data.entity_id) {

          insert_status = context.mx_entity_id_to_node_id.insert(
              {aliased_entity_id, node.node_id});

          is_duplicate = !insert_status.second;
          if (is_duplicate) {
            node.opt_aliased_entity_id = aliased_entity_id;
          }
        }
      }

      if (node.opt_aliased_entity_id.has_value()) {
        node.state = Context::Node::State::Opened;
      }

      // Save all the generated columns
      for (std::size_t i{}; i < context.column_title_list.size(); ++i) {
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
      for (auto processed_node_id : processed_node_id_list) {
        // Get the node
        auto opt_dereferenced_node_id =
            DereferenceNodeID(context, processed_node_id);

        if (!opt_dereferenced_node_id.has_value()) {
          // This is a deduplicated node, but we can't access it yet
          continue;
        }

        processed_node_id = opt_dereferenced_node_id.value();

        auto node_ptr = GetNodeFromID(context, processed_node_id);
        if (node_ptr == nullptr) {
          continue;
        }

        // Start a new expansion thread for this node
        auto &node = *node_ptr;
        if (node.state != Context::Node::State::Unopened) {
          continue;
        }

        if (node.data.entity_id == kInvalidEntityId) {
          continue;
        }

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

std::optional<TreeExplorerModel::Context::Node::ID>
TreeExplorerModel::DereferenceNodeID(const Context &context,
                                     Context::Node::ID node_id) {

  for (;;) {
    auto tree_it = context.tree.find(node_id);
    if (tree_it == context.tree.end()) {
      return node_id;
    }

    const auto &node = tree_it->second;
    if (!node.opt_aliased_entity_id.has_value()) {
      return node_id;
    }

    auto opt_node_id =
        GetNodeIDFromEntityID(context, node.opt_aliased_entity_id.value());

    if (!opt_node_id.has_value()) {
      return std::nullopt;
    }

    node_id = opt_node_id.value();
  }
}

std::optional<TreeExplorerModel::Context::Node::ID>
TreeExplorerModel::GetNodeIDFromEntityID(
    const TreeExplorerModel::Context &context, const RawEntityId &entity_id) {

  auto node_id_it = context.mx_entity_id_to_node_id.find(entity_id);
  if (node_id_it == context.mx_entity_id_to_node_id.end()) {
    return std::nullopt;
  }

  const auto &node_id = node_id_it->second;
  return node_id;
}

bool TreeExplorerModel::ImportContextSubtree(
    Context &dest_context, const Context &source_context,
    const Context::Node::ID &root_node_id) {

  //
  // User story: the user is browsing a potentially
  // big tree explorer from start to finish, occasionally
  // extracting subtress
  //
  // The reason we are copying nodes directly is:
  // 1. When the user extracts a subtree, he expects to have
  //    the same amount of nodes in the new model. We can't
  //    easily guarantee that without a. counting the length
  //    of each branch and b. scheduling all the needed requests
  // 2. In this scenario, the user is potentially extracting
  //    many subtrees, and it would not be a great UX to have
  //    that many threads running
  // 3. Given how we would have to copy things 1:1, in the worst
  //    case scenario we'd have to descend one level at a time, and
  //    wait for each request to come back. This would be really slow
  //    for the user
  //

  // Initialize the context right now, since we need a valid
  // internal root node to relocate `root_node_id`
  InitializeContext(dest_context, source_context.column_title_list);

  // Collect the nodes we need from the source context
  std::vector<Context::Node::ID> next_node_id_queue{root_node_id};
  Context::Node::ID highest_node_id{};

  while (!next_node_id_queue.empty()) {
    auto node_id_queue = std::move(next_node_id_queue);
    next_node_id_queue.clear();

    for (const auto &node_id : node_id_queue) {
      // Search for the highest node id, so we can then use it as a
      // starting point for the node id generator in `dest_context`
      highest_node_id = std::max(node_id, highest_node_id);

      // Look for the source node, and check its parent node id
      auto node_it = source_context.tree.find(node_id);
      if (node_it == source_context.tree.end()) {
        qDebug()
            << "The specified subtree contains node IDs that could not be found in the source context";

        return false;
      }

      auto node = node_it->second;

      // Relocate the root node to the correct position
      if (node.node_id == root_node_id) {
        node.parent_node_id = Context::Node::kRootNodeID;

        dest_context.tree[node.parent_node_id].child_node_id_list.push_back(
            node.node_id);
      }

      dest_context.mx_entity_id_to_node_id.insert(
          {node.data.entity_id, node.node_id});

      // Refill the queue
      next_node_id_queue.insert(next_node_id_queue.end(),
                                node.child_node_id_list.begin(),
                                node.child_node_id_list.end());

      // Before we insert this node, ensure that the parent node id
      // is valid
      if (dest_context.tree.count(node.parent_node_id) == 0) {
        qDebug()
            << "The specified subtree contains a node that references an invalid parent node";

        return false;
      }

      dest_context.tree.insert({node.node_id, std::move(node)});
    }
  }

  // Ensure we use a good initial value for the node id generator
  dest_context.node_id_generator = highest_node_id + 1;

  // Copy the tree name
  dest_context.tree_name = source_context.tree_name;

  return true;
}

}  // namespace mx::gui
