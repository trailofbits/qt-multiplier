/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "HighlightingModelProxy.h"

namespace mx::gui {

struct HighlightingModelProxy::PrivateData final {
  QAbstractItemModel *source_model{nullptr};
  int entity_id_data_role{};

  Context context;
};

HighlightingModelProxy::HighlightingModelProxy(QAbstractItemModel *source_model,
                                               const int &entity_id_data_role)
    : QAbstractItemModel(source_model->parent()),
      d(new PrivateData) {

  d->source_model = source_model;
  d->source_model->setParent(this);

  d->entity_id_data_role = entity_id_data_role;

  connect(d->source_model, &QAbstractItemModel::columnsInserted, this,
          &HighlightingModelProxy::OnColumnsInserted);

  connect(d->source_model, &QAbstractItemModel::columnsMoved, this,
          &HighlightingModelProxy::OnColumnsMoved);

  connect(d->source_model, &QAbstractItemModel::columnsRemoved, this,
          &HighlightingModelProxy::OnColumnsRemoved);

  connect(d->source_model, &QAbstractItemModel::dataChanged, this,
          &HighlightingModelProxy::OnDataChanged);

  connect(d->source_model, &QAbstractItemModel::headerDataChanged, this,
          &HighlightingModelProxy::OnHeaderDataChanged);

  connect(d->source_model, &QAbstractItemModel::layoutChanged, this,
          &HighlightingModelProxy::OnLayoutChanged);

  connect(d->source_model, &QAbstractItemModel::modelReset, this,
          &HighlightingModelProxy::OnModelReset);

  connect(d->source_model, &QAbstractItemModel::rowsInserted, this,
          &HighlightingModelProxy::OnRowsInserted);

  connect(d->source_model, &QAbstractItemModel::rowsMoved, this,
          &HighlightingModelProxy::OnRowsMoved);

  connect(d->source_model, &QAbstractItemModel::rowsRemoved, this,
          &HighlightingModelProxy::OnRowsRemoved);
}

HighlightingModelProxy::~HighlightingModelProxy() {}

QModelIndex HighlightingModelProxy::index(int row, int column,
                                          const QModelIndex &parent) const {
  Context::Node::ID parent_id{};
  if (parent.isValid()) {
    parent_id = parent.internalId();
  }

  const auto &parent_node = d->context.node_map[parent_id];

  auto child_id_it = std::next(parent_node.child_id_list.begin(), row);
  if (child_id_it >= parent_node.child_id_list.end()) {
    return QModelIndex();
  }

  const auto &child_id = *child_id_it;
  return createIndex(row, column, child_id);
}

QModelIndex HighlightingModelProxy::parent(const QModelIndex &child) const {
  if (!child.isValid()) {
    return QModelIndex();
  }

  Context::Node::ID parent_id{};

  {
    auto node_id = static_cast<Context::Node::ID>(child.internalId());
    const auto &node = d->context.node_map[node_id];

    parent_id = node.parent_id;
  }

  if (parent_id == 0) {
    return QModelIndex();
  }

  Context::Node::ID grandparent_id{};

  {
    const auto &node = d->context.node_map[parent_id];
    grandparent_id = node.parent_id;
  }

  if (grandparent_id != 0) {
    return QModelIndex();
  }

  const auto &grandparent_node = d->context.node_map[grandparent_id];

  auto it = std::find(grandparent_node.child_id_list.begin(),
                      grandparent_node.child_id_list.end(), parent_id);

  if (it == grandparent_node.child_id_list.end()) {
    return QModelIndex();
  }

  auto parent_row = static_cast<int>(
      std::distance(grandparent_node.child_id_list.begin(), it));

  return createIndex(parent_row, 0, parent_id);
}

int HighlightingModelProxy::rowCount(const QModelIndex &parent) const {
  Context::Node::ID parent_id{};
  if (parent.isValid()) {
    parent_id = parent.internalId();
  }

  const auto &parent_node = d->context.node_map[parent_id];
  return d->source_model->rowCount(parent_node.source_index);
}

int HighlightingModelProxy::columnCount(const QModelIndex &parent) const {
  Context::Node::ID parent_id{};
  if (parent.isValid()) {
    parent_id = parent.internalId();
  }

  const auto &parent_node = d->context.node_map[parent_id];
  return d->source_model->columnCount(parent_node.source_index);
}

QVariant HighlightingModelProxy::data(const QModelIndex &index,
                                      int role) const {
  Context::Node::ID child_id{};
  if (index.isValid()) {
    child_id = index.internalId();
  }

  const auto &child = d->context.node_map[child_id];

  auto parent_index = child.source_index.parent();
  auto column_index =
      d->source_model->index(index.row(), index.column(), parent_index);

  if (role == Qt::BackgroundRole) {
    auto var = column_index.data(role);
    if (var.isValid()) {
      return var;
    }

    auto raw_entity_id_var = column_index.data(d->entity_id_data_role);
    if (!raw_entity_id_var.isValid()) {
      return QVariant();
    }

    auto raw_entity_id = qvariant_cast<RawEntityId>(raw_entity_id_var);

    auto highlight_it = d->context.entity_highlight_list.find(raw_entity_id);
    if (highlight_it == d->context.entity_highlight_list.end()) {
      return QVariant();
    }

    const auto &highlight_color = highlight_it->second;
    return highlight_color;
  }

  return column_index.data(role);
}

HighlightingModelProxy::Context::Node::ID
HighlightingModelProxy::GenerateNodeID(Context &context) {
  return ++context.id_generator;
}

void GenerateModelIndexHelper(
    HighlightingModelProxy::Context &context,
    const QAbstractItemModel &source_model, const QModelIndex &parent,
    const HighlightingModelProxy::Context::Node::ID &parent_id) {

  auto row_count = source_model.rowCount(parent);

  for (int row{}; row < row_count; ++row) {
    auto child_node_id = HighlightingModelProxy::GenerateNodeID(context);

    auto &parent_node = context.node_map[parent_id];
    parent_node.child_id_list.push_back(child_node_id);

    auto child_index = source_model.index(row, 0, parent);

    HighlightingModelProxy::Context::Node child_node;
    child_node.id = child_node_id;
    child_node.parent_id = parent_id;
    child_node.source_index = child_index;

    context.node_map.insert({child_node_id, std::move(child_node)});

    GenerateModelIndexHelper(context, source_model, child_index, child_node_id);
  }
}

void HighlightingModelProxy::GenerateModelIndex(
    Context &context, const QAbstractItemModel &source_model) {

  context.id_generator = 0;
  context.node_map = {{0, {0, 0, QModelIndex(), {}}}};

  GenerateModelIndexHelper(context, source_model, QModelIndex(), 0);
}

void HighlightingModelProxy::OnEntityHighlightListChange(
    const EntityHighlightList &entity_highlight_list) {

  d->context.entity_highlight_list = entity_highlight_list;

  emit dataChanged(QModelIndex(), QModelIndex(), {Qt::BackgroundRole});
}

void HighlightingModelProxy::OnColumnsInserted(const QModelIndex &, int, int) {
  GenerateModelIndex();
}

void HighlightingModelProxy::OnColumnsMoved(const QModelIndex &, int, int,
                                            const QModelIndex &, int) {
  GenerateModelIndex();
}

void HighlightingModelProxy::OnColumnsRemoved(const QModelIndex &, int, int) {
  GenerateModelIndex();
}

void HighlightingModelProxy::OnDataChanged(const QModelIndex &,
                                           const QModelIndex &,
                                           const QList<int> &) {
  GenerateModelIndex();
}

void HighlightingModelProxy::OnHeaderDataChanged(Qt::Orientation, int, int) {
  GenerateModelIndex();
}

void HighlightingModelProxy::OnLayoutChanged(
    const QList<QPersistentModelIndex> &,
    QAbstractItemModel::LayoutChangeHint) {
  GenerateModelIndex();
}

void HighlightingModelProxy::OnModelReset() {
  GenerateModelIndex();
}

void HighlightingModelProxy::OnRowsInserted(const QModelIndex &, int, int) {
  GenerateModelIndex();
}

void HighlightingModelProxy::OnRowsMoved(const QModelIndex &, int, int,
                                         const QModelIndex &, int) {
  GenerateModelIndex();
}

void HighlightingModelProxy::OnRowsRemoved(const QModelIndex &, int, int) {
  GenerateModelIndex();
}

void HighlightingModelProxy::GenerateModelIndex() {
  emit beginResetModel();

  GenerateModelIndex(d->context, *d->source_model);

  emit endResetModel();
}

}  // namespace mx::gui
