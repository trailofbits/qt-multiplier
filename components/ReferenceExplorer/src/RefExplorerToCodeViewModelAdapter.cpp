/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "Types.h"
#include "RefExplorerToCodeViewModelAdapter.h"

#include <multiplier/ui/IReferenceExplorerModel.h>
#include <multiplier/ui/Assert.h>

#include <filesystem>
#include <iostream>

namespace mx::gui {

namespace {

static const QString kExpandText = "[+]";

void AppendIndentWhitespace(QString &buffer, std::size_t level_count) {
  for (std::size_t i{}; i < level_count; ++i) {
    buffer.append(QChar::Space);
    buffer.append(QChar::Space);
  }
}

void ImportReferenceExplorerModelHelper(
    RefExplorerToCodeViewModelAdapter::Context &context,
    const QAbstractItemModel *model, const QModelIndex &root,
    const std::size_t &indent, std::size_t &line_number) {

  auto L_getBreadcrumbs =
      [model](const QModelIndex &index) -> std::optional<QString> {
    auto breadcrumbs_index = model->index(index.row(), 2, index.parent());

    auto breadcrumbs_var = breadcrumbs_index.data();
    if (!breadcrumbs_var.isValid()) {
      return std::nullopt;
    }

    const auto &breadcrumbs = breadcrumbs_var.toString();
    if (breadcrumbs.isEmpty()) {
      return std::nullopt;
    }

    return breadcrumbs;
  };

  auto node_name_var = root.data();
  if (!node_name_var.isValid()) {
    return;
  }

  const auto &node_name = node_name_var.toString();

  auto token_category{TokenCategory::UNKNOWN};
  auto token_category_var =
      root.data(IReferenceExplorerModel::TokenCategoryRole);

  if (token_category_var.isValid()) {
    token_category = qvariant_cast<TokenCategory>(token_category_var);
  }

  QString symbol{"?"};
  if (auto expansion_mode_var{
          root.data(IReferenceExplorerModel::ExpansionModeRole)};
      expansion_mode_var.isValid()) {

    auto expansion_mode = qvariant_cast<IReferenceExplorerModel::ExpansionMode>(
        expansion_mode_var);

    switch (expansion_mode) {
      case IReferenceExplorerModel::ExpansionMode::CallHierarchyMode:
        symbol = "x";
        break;

      case IReferenceExplorerModel::ExpansionMode::TaintMode:
        symbol = "t";
        break;
    }
  }

  RefExplorerToCodeViewModelAdapter::Context::Node::ColumnListData
      column_list_data;

  auto L_generateIndent = [&](const std::size_t &indent) {
    RefExplorerToCodeViewModelAdapter::Context::Node::ColumnListData::Column
        column;
    AppendIndentWhitespace(column.data, indent);

    column_list_data.column_list.push_back(std::move(column));
  };

  auto L_generateColumn = [&](const QString &data,
                              const TokenCategory &token_category) {
    RefExplorerToCodeViewModelAdapter::Context::Node::ColumnListData::Column
        column;

    column.data = data;
    column.token_category = token_category;
    column.is_expand_button =
        (data == kExpandText && token_category == TokenCategory::COMMENT);

    column_list_data.column_list.push_back(std::move(column));
  };

  auto L_saveCurrentLine = [&]() {
    auto line_node_id =
        RefExplorerToCodeViewModelAdapter::GenerateNodeID(context);

    auto column_list_node_id =
        RefExplorerToCodeViewModelAdapter::GenerateNodeID(context);

    RefExplorerToCodeViewModelAdapter::Context::Node::LineData line_data;
    line_data.original_model_index = root;
    line_data.child_id = column_list_node_id;
    line_data.line_number = line_number;

    RefExplorerToCodeViewModelAdapter::Context::Node line_node;
    line_node.id = line_node_id;
    line_node.parent_id = 0;
    line_node.data = std::move(line_data);

    RefExplorerToCodeViewModelAdapter::Context::Node column_list_node;
    column_list_node.id = column_list_node_id;
    column_list_node.parent_id = line_node_id;
    column_list_node.data = std::move(column_list_data);

    column_list_data = {};

    context.node_map.insert({line_node_id, std::move(line_node)});
    context.node_map.insert({column_list_node_id, std::move(column_list_node)});

    auto &root_node = context.node_map[0];
    auto &root_data =
        std::get<RefExplorerToCodeViewModelAdapter::Context::Node::RootData>(
            root_node.data);

    root_data.child_id_list.push_back(line_node_id);

    ++line_number;
  };

  if (context.breadcrumbs_enabled) {
    if (auto opt_breadcrumbs = L_getBreadcrumbs(root);
        opt_breadcrumbs.has_value()) {

      const auto &breadcrumbs = opt_breadcrumbs.value();

      L_generateIndent(indent + 1);
      L_generateColumn(breadcrumbs, TokenCategory::COMMENT);
      L_saveCurrentLine();
    }
  }

  L_generateIndent(indent);
  L_generateColumn(symbol, TokenCategory::COMMENT);
  L_generateColumn(" ", TokenCategory::WHITESPACE);
  L_generateColumn(node_name, token_category);

  auto location_var = root.data(IReferenceExplorerModel::LocationRole);
  if (location_var.isValid()) {
    const auto &location = qvariant_cast<Location>(location_var);
    std::filesystem::path path{location.path.toStdString()};

    L_generateColumn(" ", TokenCategory::WHITESPACE);
    L_generateColumn(QString::fromStdString(path.filename().generic_string()),
                     TokenCategory::FILE_NAME);

    L_generateColumn(":", TokenCategory::PUNCTUATION);
    L_generateColumn(QString::number(location.line),
                     TokenCategory::LINE_NUMBER);

    L_generateColumn(":", TokenCategory::PUNCTUATION);
    L_generateColumn(QString::number(location.column),
                     TokenCategory::COLUMN_NUMBER);
  }

  auto show_expand_comment{true};
  auto expansion_status_role_var =
      root.data(IReferenceExplorerModel::ExpansionStatusRole);

  if (expansion_status_role_var.isValid()) {
    auto already_expanded = expansion_status_role_var.toBool();
    show_expand_comment = !already_expanded;
  }

  if (show_expand_comment) {
    L_generateColumn(" ", TokenCategory::WHITESPACE);
    L_generateColumn(kExpandText, TokenCategory::COMMENT);
  }

  L_saveCurrentLine();

  auto row_count = model->rowCount(root);

  for (int row_num{0}; row_num < row_count; ++row_num) {
    auto child_index = model->index(row_num, 0, root);
    if (!child_index.isValid()) {
      continue;
    }

    ImportReferenceExplorerModelHelper(context, model, child_index, indent + 1,
                                       line_number);
  }
}

}  // namespace

struct RefExplorerToCodeViewModelAdapter::PrivateData final {
  QAbstractItemModel *model{nullptr};
  Context context;
};

RefExplorerToCodeViewModelAdapter::RefExplorerToCodeViewModelAdapter(
    QAbstractItemModel *model, QObject *parent)
    : ICodeModel(parent),
      d(new PrivateData) {

  d->model = model;

  connect(d->model, &QAbstractItemModel::modelReset, this,
          &RefExplorerToCodeViewModelAdapter::OnModelChange);

  connect(d->model, &QAbstractItemModel::dataChanged, this,
          &RefExplorerToCodeViewModelAdapter::OnModelChange);

  connect(d->model, &QAbstractItemModel::rowsInserted, this,
          &RefExplorerToCodeViewModelAdapter::OnModelChange);

  OnModelChange();
}

RefExplorerToCodeViewModelAdapter::~RefExplorerToCodeViewModelAdapter() {}

void RefExplorerToCodeViewModelAdapter::SetBreadcrumbsVisibility(
    const bool &enable) {

  emit beginResetModel();

  d->context.breadcrumbs_enabled = enable;
  ImportReferenceExplorerModel(d->context, d->model);

  emit endResetModel();
}

std::optional<RawEntityId>
RefExplorerToCodeViewModelAdapter::GetEntity() const {
  Assert(
      false,
      "Invalid virtual method call: RefExplorerToCodeViewModelAdapter::GetEntity");

  __builtin_unreachable();
}

void RefExplorerToCodeViewModelAdapter::SetEntity(RawEntityId) {
  Assert(
      false,
      "Invalid virtual method call: RefExplorerToCodeViewModelAdapter::SetEntity");

  __builtin_unreachable();
}

bool RefExplorerToCodeViewModelAdapter::IsReady() const {
  return true;
}

QModelIndex
RefExplorerToCodeViewModelAdapter::index(int row, int column,
                                         const QModelIndex &parent) const {

  Context::Node::ID parent_node_id{};
  if (parent.isValid()) {
    parent_node_id = static_cast<Context::Node::ID>(parent.internalId());
  }

  const auto &parent_node = d->context.node_map[parent_node_id];
  if (std::holds_alternative<Context::Node::RootData>(parent_node.data)) {
    if (column != 0) {
      return QModelIndex();
    }

    auto unsigned_row = static_cast<std::size_t>(row);

    const auto &root_data = std::get<Context::Node::RootData>(parent_node.data);
    if (unsigned_row >= root_data.child_id_list.size()) {
      return QModelIndex();
    }

    const auto &child_id = root_data.child_id_list[unsigned_row];
    return createIndex(row, column, child_id);

  } else if (std::holds_alternative<Context::Node::LineData>(
                 parent_node.data)) {
    if (row != 0) {
      return QModelIndex();
    }

    const auto &line_data = std::get<Context::Node::LineData>(parent_node.data);
    return createIndex(row, column, line_data.child_id);

  } else {
    return QModelIndex();
  }
}

QModelIndex
RefExplorerToCodeViewModelAdapter::parent(const QModelIndex &child) const {
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

  if (!std::holds_alternative<Context::Node::RootData>(grandparent_node.data)) {
    return QModelIndex();
  }

  const auto &root_data =
      std::get<Context::Node::RootData>(grandparent_node.data);

  auto it = std::find(root_data.child_id_list.begin(),
                      root_data.child_id_list.end(), parent_id);

  if (it == root_data.child_id_list.end()) {
    return QModelIndex();
  }

  auto parent_row =
      static_cast<int>(std::distance(root_data.child_id_list.begin(), it));

  return createIndex(parent_row, 0, parent_id);
}

int RefExplorerToCodeViewModelAdapter::rowCount(
    const QModelIndex &parent) const {

  Context::Node::ID parent_node_id{};
  if (parent.isValid()) {
    parent_node_id = static_cast<Context::Node::ID>(parent.internalId());
  }

  const auto &parent_node = d->context.node_map[parent_node_id];
  if (std::holds_alternative<Context::Node::RootData>(parent_node.data)) {
    const auto &root_data = std::get<Context::Node::RootData>(parent_node.data);
    return static_cast<int>(root_data.child_id_list.size());

  } else if (std::holds_alternative<Context::Node::LineData>(
                 parent_node.data)) {
    return 1;

  } else {
    return 0;
  }
}

int RefExplorerToCodeViewModelAdapter::columnCount(
    const QModelIndex &parent) const {

  Context::Node::ID parent_node_id{};
  if (parent.isValid()) {
    parent_node_id = static_cast<Context::Node::ID>(parent.internalId());
  }

  const auto &parent_node = d->context.node_map[parent_node_id];
  if (std::holds_alternative<Context::Node::LineData>(parent_node.data)) {
    const auto &line_data = std::get<Context::Node::LineData>(parent_node.data);

    const auto &column_list_node = d->context.node_map[line_data.child_id];
    const auto &column_list_data =
        std::get<Context::Node::ColumnListData>(column_list_node.data);

    return static_cast<int>(column_list_data.column_list.size());
  }

  return 1;
}

QVariant RefExplorerToCodeViewModelAdapter::data(const QModelIndex &index,
                                                 int role) const {

  auto node_id = static_cast<Context::Node::ID>(index.internalId());
  const auto &node = d->context.node_map[node_id];

  QVariant value;

  if (std::holds_alternative<Context::Node::LineData>(node.data)) {
    const auto &line_data = std::get<Context::Node::LineData>(node.data);

    if (role == Qt::DisplayRole) {
      value.setValue(QString::number(line_data.line_number));

    } else if (role == ICodeModel::LineNumberRole) {
      value.setValue(line_data.line_number);

    } else if (role == RefExplorerToCodeViewModelAdapter::OriginalModelIndex) {
      value.setValue(line_data.original_model_index);
    }

  } else if (std::holds_alternative<Context::Node::ColumnListData>(node.data)) {
    const auto &column_list_data =
        std::get<Context::Node::ColumnListData>(node.data);

    auto unsigned_column = static_cast<std::size_t>(index.column());
    if (unsigned_column >= column_list_data.column_list.size()) {
      return QVariant();
    }

    const auto &column = column_list_data.column_list[unsigned_column];

    if (role == Qt::DisplayRole) {
      value.setValue(column.data);

    } else if (role == ICodeModel::TokenCategoryRole) {
      value.setValue(static_cast<std::uint32_t>(column.token_category));

    } else if (role == ICodeModel::LineNumberRole) {
      auto parent_index = index.parent();
      value = parent_index.data(ICodeModel::LineNumberRole);

    } else if (role == RefExplorerToCodeViewModelAdapter::OriginalModelIndex) {
      auto parent_index = index.parent();
      value = parent_index.data(role);

    } else if (role == RefExplorerToCodeViewModelAdapter::IsExpandButton) {
      value.setValue(column.is_expand_button);

    } else if (role == Qt::ForegroundRole || role == Qt::BackgroundRole) {
      auto parent_index = index.parent();

      auto original_index_var = parent_index.data(
          RefExplorerToCodeViewModelAdapter::OriginalModelIndex);

      if (original_index_var.isValid()) {
        const auto &original_index =
            qvariant_cast<QModelIndex>(original_index_var);

        value = original_index.data(role);
      }
    }
  }

  return value;
}

void RefExplorerToCodeViewModelAdapter::ImportReferenceExplorerModel(
    Context &context, const QAbstractItemModel *model) {

  context.node_id_generator = 0;
  context.node_map.clear();

  Context::Node root_node;
  root_node.data = Context::Node::RootData{};
  context.node_map.insert({0, std::move(root_node)});

  auto row_count = model->rowCount();
  std::size_t line_number{1};

  for (int row{0}; row < row_count; ++row) {
    auto child_index = model->index(row, 0);

    ImportReferenceExplorerModelHelper(context, model, child_index, 0,
                                       line_number);
  }
}

RefExplorerToCodeViewModelAdapter::Context::Node::ID
RefExplorerToCodeViewModelAdapter::GenerateNodeID(Context &context) {

  return ++context.node_id_generator;
}

void RefExplorerToCodeViewModelAdapter::OnModelChange() {
  emit beginResetModel();

  ImportReferenceExplorerModel(d->context, d->model);

  emit endResetModel();
}

}  // namespace mx::gui
