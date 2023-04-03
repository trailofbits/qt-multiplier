/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "Types.h"
#include "RefExplorerToCodeViewModelAdapter.h"

#include <multiplier/ui/Assert.h>

#include <filesystem>

namespace mx::gui {

namespace {

void GenerateToken(
    RefExplorerToCodeViewModelAdapter::Context::TokenList &token_list,
    std::uint64_t &id_generator, const TokenCategory &token_category,
    const QString &data) {
  token_list.push_back({data, token_category, ++id_generator});
}

void AppendIndentWhitespace(QString &buffer, const std::size_t &level_count) {
  static const QString kIndent{"  "};

  for (std::size_t i{}; i < level_count; ++i) {
    buffer.append(kIndent);
  }
}

void GenerateIndentToken(
    RefExplorerToCodeViewModelAdapter::Context::TokenList &token_list,
    std::uint64_t &id_generator, const std::size_t &indent) {

  QString token_data;
  AppendIndentWhitespace(token_data, indent);

  GenerateToken(token_list, id_generator, TokenCategory::UNKNOWN, token_data);
}

void ImportReferenceExplorerModelHelper(
    RefExplorerToCodeViewModelAdapter::Context &context,
    const IReferenceExplorerModel *model, const QModelIndex &root,
    std::uint64_t &id_generator, const std::size_t &indent) {

  auto node_name_var = root.data();
  if (!node_name_var.isValid()) {
    return;
  }

  const auto &node_name = node_name_var.toString();

  RefExplorerToCodeViewModelAdapter::Context::Row row;
  row.original_model_index = root;

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

  GenerateIndentToken(row.token_list, id_generator, indent);
  GenerateToken(row.token_list, id_generator, TokenCategory::COMMENT, symbol);
  GenerateToken(row.token_list, id_generator, TokenCategory::UNKNOWN, " ");
  GenerateToken(row.token_list, id_generator, token_category, node_name);

  auto location_var = root.data(IReferenceExplorerModel::LocationRole);
  if (location_var.isValid()) {
    const auto &location = qvariant_cast<Location>(location_var);

    GenerateToken(row.token_list, id_generator, TokenCategory::UNKNOWN, " ");

    std::filesystem::path path{location.path.toStdString()};
    auto location_token = path.filename().string() + "@" +
                          std::to_string(location.line) + ":" +
                          std::to_string(location.column);

    GenerateToken(row.token_list, id_generator, TokenCategory::NAMESPACE,
                  QString::fromStdString(location_token));
  }

  auto show_expand_comment{true};
  auto expansion_status_role_var =
      root.data(IReferenceExplorerModel::ExpansionStatusRole);
  if (expansion_status_role_var.isValid()) {
    auto already_expanded = expansion_status_role_var.toBool();
    show_expand_comment = !already_expanded;
  }

  if (show_expand_comment) {
    GenerateToken(row.token_list, id_generator, TokenCategory::UNKNOWN, " ");
    GenerateToken(row.token_list, id_generator, TokenCategory::COMMENT, "[+]");
  }

  context.row_list.push_back(std::move(row));

  auto row_count = model->rowCount(root);

  for (int row_num{0}; row_num < row_count; ++row_num) {
    auto child_index = model->index(row_num, 0, root);
    if (!child_index.isValid()) {
      continue;
    }

    ImportReferenceExplorerModelHelper(context, model, child_index,
                                       id_generator, indent + 1);
  }
}

}  // namespace

struct RefExplorerToCodeViewModelAdapter::PrivateData final {
  IReferenceExplorerModel *model{nullptr};
  Context context;
};

RefExplorerToCodeViewModelAdapter::RefExplorerToCodeViewModelAdapter(
    IReferenceExplorerModel *model, QObject *parent)
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

const FileLocationCache &
RefExplorerToCodeViewModelAdapter::GetFileLocationCache() const {
  Assert(
      false,
      "Invalid virtual method call: RefExplorerToCodeViewModelAdapter::GetFileLocationCache");

  __builtin_unreachable();
}

Index &RefExplorerToCodeViewModelAdapter::GetIndex() {
  Assert(
      false,
      "Invalid virtual method call: RefExplorerToCodeViewModelAdapter::GetIndex");

  __builtin_unreachable();
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

int RefExplorerToCodeViewModelAdapter::RowCount() const {
  return static_cast<int>(d->context.row_list.size());
}

int RefExplorerToCodeViewModelAdapter::TokenCount(int row) const {
  auto row_number = static_cast<std::size_t>(row);
  if (row_number >= d->context.row_list.size()) {
    return 0;
  }

  const auto &r = d->context.row_list[row_number];
  return static_cast<int>(r.token_list.size());
}

QVariant RefExplorerToCodeViewModelAdapter::Data(const CodeModelIndex &index,
                                                 int role) const {
  if (role == ICodeModel::LineNumberRole) {
    return index.row + 1;
  }

  auto row_number = static_cast<std::size_t>(index.row);
  if (row_number >= d->context.row_list.size()) {
    return QVariant();
  }

  const auto &row = d->context.row_list[row_number];

  auto token_number = static_cast<std::size_t>(index.token_index);
  if (token_number >= row.token_list.size()) {
    return QVariant();
  }

  const auto &token = row.token_list[token_number];

  QVariant value;
  if (role == Qt::DisplayRole) {
    value.setValue(token.data);

  } else if (role == ICodeModel::TokenCategoryRole) {
    value.setValue(token.category);

  } else if (role == RefExplorerToCodeViewModelAdapter::OriginalModelIndex) {
    value.setValue(row.original_model_index);
  }

  return value;
}

bool RefExplorerToCodeViewModelAdapter::IsReady() const {
  return true;
}

void RefExplorerToCodeViewModelAdapter::ImportReferenceExplorerModel(
    Context &context, const IReferenceExplorerModel *model) {

  context = {};

  std::uint64_t id_generator{};
  auto row_count = model->rowCount();

  for (int row{0}; row < row_count; ++row) {
    auto child_index = model->index(row, 0);

    ImportReferenceExplorerModelHelper(context, model, child_index,
                                       id_generator, 0);
  }
}

void RefExplorerToCodeViewModelAdapter::OnModelChange() {
  ImportReferenceExplorerModel(d->context, d->model);
  emit ModelReset();
}

}  // namespace mx::gui
