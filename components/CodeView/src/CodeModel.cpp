/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "CodeModel.h"

#include <multiplier/ui/IDatabase.h>

#include <QFutureWatcher>

#include <unordered_set>

namespace mx::gui {

namespace {

enum class ModelState {
  UpdateInProgress,
  UpdateFailed,
  UpdateCancelled,
  Ready,
};

}

struct CodeModel::PrivateData final {
  PrivateData(const FileLocationCache &file_location_cache_,
              const Index &index_)
      : file_location_cache(file_location_cache_),
        index(index_) {}

  FileLocationCache file_location_cache;
  Index index;

  ModelState model_state{ModelState::Ready};
  std::optional<RawEntityId> opt_entity_id;

  IDatabase::Ptr database;
  QFuture<IDatabase::IndexedTokenRangeDataResult> future_result;
  QFutureWatcher<IDatabase::IndexedTokenRangeDataResult> future_watcher;

  Context context;
};

CodeModel::~CodeModel() {
  CancelRunningRequest();
}

std::optional<RawEntityId> CodeModel::GetEntity() const {
  return d->opt_entity_id;
}

void CodeModel::SetEntity(RawEntityId raw_id) {
  CancelRunningRequest();

  emit beginResetModel();
  d->opt_entity_id = std::nullopt;

  EntityId eid(raw_id);
  VariantId vid = eid.Unpack();

  if (std::holds_alternative<FileId>(vid)) {
    d->future_result = d->database->RequestIndexedTokenRangeData(
        raw_id, IDatabase::IndexedTokenRangeDataRequestType::File);

  } else if (std::holds_alternative<FileTokenId>(vid)) {
    d->future_result = d->database->RequestIndexedTokenRangeData(
        raw_id, IDatabase::IndexedTokenRangeDataRequestType::File);

  } else if (std::optional<FragmentId> frag_id = FragmentId::from(eid)) {
    d->future_result = d->database->RequestIndexedTokenRangeData(
        EntityId(frag_id.value()).Pack(),
        IDatabase::IndexedTokenRangeDataRequestType::Fragment);

  } else {
    d->model_state = ModelState::UpdateFailed;
    emit endResetModel();

    return;
  }

  d->model_state = ModelState::UpdateInProgress;
  d->future_watcher.setFuture(d->future_result);

  emit endResetModel();
}

bool CodeModel::IsReady() const {
  return d->model_state == ModelState::Ready;
}

QModelIndex CodeModel::index(int row, int column,
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

QModelIndex CodeModel::parent(const QModelIndex &child) const {
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

int CodeModel::rowCount(const QModelIndex &parent) const {
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

int CodeModel::columnCount(const QModelIndex &parent) const {
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

QVariant CodeModel::data(const QModelIndex &index, int role) const {
  auto node_id = static_cast<Context::Node::ID>(index.internalId());
  const auto &node = d->context.node_map[node_id];

  QVariant value;

  if (std::holds_alternative<Context::Node::LineData>(node.data)) {
    const auto &line_data = std::get<Context::Node::LineData>(node.data);

    if (role == Qt::DisplayRole) {
      value.setValue(QString::number(line_data.line_number));

    } else if (role == ICodeModel::LineNumberRole) {
      value.setValue(line_data.line_number);
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

    } else if (role == ICodeModel::TokenIdRole) {
      value.setValue(static_cast<std::uint64_t>(column.token_id));

    } else if (role == ICodeModel::LineNumberRole) {
      auto parent_index = index.parent();
      value = parent_index.data(ICodeModel::LineNumberRole);

    } else if (role == ICodeModel::TokenGroupIdRole) {
      if (column.opt_token_group_id.has_value()) {
        value.setValue(column.opt_token_group_id.value());
      }

    } else if (role == ICodeModel::RelatedEntityIdRole ||
               role == ICodeModel::RealRelatedEntityIdRole) {
      if (column.related_entity_id != kInvalidEntityId) {
        value.setValue(static_cast<std::uint64_t>(column.related_entity_id));
      }

    } else if (role == ICodeModel::EntityIdOfStmtContainingTokenRole) {
      if (column.statement_entity_id != kInvalidEntityId) {
        value.setValue(static_cast<std::uint64_t>(column.statement_entity_id));
      }
    }
  }

  return value;
}

CodeModel::Context::Node::ID CodeModel::GenerateNodeID(Context &context) {
  return ++context.node_id_generator;
}

CodeModel::CodeModel(const FileLocationCache &file_location_cache,
                     const Index &index, QObject *parent)
    : ICodeModel(parent),
      d(new PrivateData(file_location_cache, index)) {

  d->database = IDatabase::Create(index, file_location_cache);

  connect(&d->future_watcher,
          &QFutureWatcher<IDatabase::IndexedTokenRangeDataResult>::finished,
          this, &CodeModel::FutureResultStateChanged);
}

void CodeModel::CancelRunningRequest() {
  if (!d->future_result.isRunning()) {
    return;
  }

  d->future_result.cancel();
  d->future_result.waitForFinished();

  d->future_result = {};
}

void CodeModel::FutureResultStateChanged() {
  emit beginResetModel();

  d->context = {};

  if (d->future_result.isCanceled()) {
    d->model_state = ModelState::UpdateCancelled;
    emit endResetModel();

    return;
  }

  auto future_result = d->future_result.takeResult();
  if (!future_result.Succeeded()) {
    d->model_state = ModelState::UpdateFailed;
    emit endResetModel();

    return;
  }

  IndexedTokenRangeData indexed_token_range_data = future_result.TakeValue();
  d->opt_entity_id = indexed_token_range_data.requested_id;

  auto token_count = indexed_token_range_data.start_of_token.size() - 1u;

  Context::Node::RootData root_data;

  std::size_t line_number{};
  Context::Node::ColumnListData column_list_data;

  auto L_saveRow = [&]() {
    auto line_node_id = GenerateNodeID(d->context);
    auto column_list_node_id = GenerateNodeID(d->context);

    // Generate a line node
    Context::Node line_node;
    line_node.id = line_node_id;
    line_node.parent_id = 0;

    Context::Node::LineData line_data;
    line_data.line_number = line_number;
    line_data.child_id = column_list_node_id;

    line_node.data = std::move(line_data);

    // Generate the column list node
    Context::Node column_list_node;
    column_list_node.id = column_list_node_id;
    column_list_node.parent_id = line_node_id;

    column_list_node.data = std::move(column_list_data);

    // Save the nodes
    d->context.node_map.insert(
        {column_list_node_id, std::move(column_list_node)});

    d->context.node_map.insert({line_node_id, std::move(line_node)});

    // Update the root node
    root_data.child_id_list.push_back(line_node_id);

    column_list_data = {};
  };

  for (std::size_t i = 0; i < token_count; ++i) {
    auto token_start = indexed_token_range_data.start_of_token[i];
    auto token_end = indexed_token_range_data.start_of_token[i + 1];
    auto token_length = token_end - token_start;

    Context::Node::ColumnListData::Column col = {};
    col.token_id = indexed_token_range_data.token_ids[i];
    col.related_entity_id = indexed_token_range_data.related_entity_ids[i];
    col.statement_entity_id =
        indexed_token_range_data.statement_containing_token[i];

    col.token_category = indexed_token_range_data.token_categories[i];

    auto frag_id_index = indexed_token_range_data.fragment_id_index[i];
    if (auto frag_id = indexed_token_range_data.fragment_ids[frag_id_index];
        frag_id != kInvalidEntityId) {
      col.opt_token_group_id.emplace(frag_id);
    }

    line_number =
        static_cast<std::size_t>(indexed_token_range_data.line_number[i]);

    qsizetype line_number_adjust = 0;
    if (indexed_token_range_data.data[token_end - 1] == QChar::LineSeparator) {
      line_number_adjust = 1;
    }

    col.data = indexed_token_range_data.data.mid(
        token_start, token_length - line_number_adjust);

    column_list_data.column_list.push_back(std::move(col));

    if (line_number_adjust) {
      L_saveRow();
    }
  }

  // Flush the last row.
  if (!column_list_data.column_list.empty()) {
    L_saveRow();
  }

  Context::Node root_node;
  root_node.data = std::move(root_data);

  d->context.node_map.insert({0, std::move(root_node)});

  d->model_state = ModelState::Ready;
  emit endResetModel();
}

}  // namespace mx::gui
