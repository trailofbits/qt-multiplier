/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "CodeModel.h"

#include <multiplier/ui/IDatabase.h>
#include <multiplier/TokenTree.h>

#include <QFutureWatcher>

#include <unordered_set>

namespace mx::gui {

struct CodeModel::PrivateData final {
  PrivateData(const FileLocationCache &file_location_cache_,
              const Index &index_)
      : file_location_cache(file_location_cache_),
        index(index_) {}

  FileLocationCache file_location_cache;
  Index index;

  const TokenTreeVisitor empty_macro_expansion_config{};
  const TokenTreeVisitor *macro_expansion_config{&empty_macro_expansion_config};

  ModelState model_state{ModelState::Uninitialized};

  const IndexedTokenRangeData::Line *last_line_cache{nullptr};
  const IndexedTokenRangeData::Column *last_column_cache{nullptr};
  IndexedTokenRangeData tokens;

  const IndexedTokenRangeData::Line *LinePointerCast(
      const QModelIndex &index);

  const IndexedTokenRangeData::Column *ColumnPointerCast(
      const QModelIndex &index);

  IDatabase::Ptr database;
  QFuture<IDatabase::IndexedTokenRangeDataResult> future_result;
  QFutureWatcher<IDatabase::IndexedTokenRangeDataResult> future_watcher;
};

const IndexedTokenRangeData::Line *CodeModel::PrivateData::LinePointerCast(
    const QModelIndex &model_index) {
  const void *ptr = model_index.constInternalPointer();
  if (last_line_cache == ptr) {
    return last_line_cache;
  }

  auto line = reinterpret_cast<const IndexedTokenRangeData::Line *>(ptr);
  if (auto num_lines = tokens.lines.size()) {
    auto first = tokens.lines.data();
    auto last = &(first[num_lines - 1u]);
    if (first <= line && line <= last) {
      last_line_cache = line;
      return line;
    }
  }
  return nullptr;
}

const IndexedTokenRangeData::Column *CodeModel::PrivateData::ColumnPointerCast(
    const QModelIndex &model_index) {
  const void *ptr = model_index.constInternalPointer();
  if (ptr == last_column_cache) {
    return last_column_cache;
  }

  unsigned row = static_cast<unsigned>(model_index.row());
  auto num_lines = tokens.lines.size();
  if (row >= num_lines) {  // Make unsafe :-P
    return nullptr;
  }

  const IndexedTokenRangeData::Line &line = tokens.lines[row];
  last_line_cache = &line;

  unsigned col = static_cast<unsigned>(model_index.column());
  auto num_cols = line.columns.size();
  if (col >= num_cols) {
    return nullptr;
  }

  auto tok = reinterpret_cast<const IndexedTokenRangeData::Column *>(ptr);
  auto first = line.columns.data();
  auto last = &(first[num_cols - 1u]);
  if (first <= tok && tok <= last) {
    last_column_cache = tok;
    return tok;
  }
  return nullptr;
}

CodeModel::~CodeModel() {
  CancelRunningRequest();
}

void CodeModel::SetEntity(RawEntityId raw_id) {
  if (d->tokens.requested_id == raw_id || d->tokens.response_id == raw_id) {
    return;  // Don't change anything.
  }

  CancelRunningRequest();

  emit beginResetModel();
  d->model_state = ModelState::UpdateInProgress;
  d->future_result = d->database->RequestIndexedTokenRangeData(
      raw_id, d->macro_expansion_config);
  d->future_watcher.setFuture(d->future_result);

  emit endResetModel();
}

bool CodeModel::IsReady() const {
  return d->model_state == ModelState::Ready;
}

QModelIndex CodeModel::index(int row, int column,
                             const QModelIndex &parent) const {
  //  if (!hasIndex(row, column, parent)) {
  //    return QModelIndex();
  //  }

  if (parent.isValid()) {
    if (row != 0) {
      return QModelIndex();
    }

    auto line = d->LinePointerCast(parent);
    if (!line) {
      return QModelIndex();
    }

    if (auto col_index = static_cast<unsigned>(column);
        col_index < line->columns.size()) {
      auto col = &(line->columns[col_index]);
      return createIndex(parent.row(), column, col);
    }
  } else {
    if (column != 0) {
      return QModelIndex();
    }

    if (auto row_index = static_cast<unsigned>(row);
        row_index < d->tokens.lines.size()) {
      return createIndex(row, 0, &(d->tokens.lines[row_index]));
    }
  }

  return QModelIndex();
}

QModelIndex CodeModel::parent(const QModelIndex &child) const {

  if (!child.isValid()) {
    return QModelIndex();

  } else if (auto col = d->ColumnPointerCast(child)) {
    auto row_index = static_cast<unsigned>(child.row());
    return createIndex(child.row(), 0, &(d->tokens.lines[row_index]));

  // Otherwise it's a line, or its the root, so give us back the root.
  } else {
    return QModelIndex();
  }
}

int CodeModel::rowCount(const QModelIndex &parent) const {
  if (!parent.isValid()) {  // Root item.
    return static_cast<int>(d->tokens.lines.size());
  } else {
    return 0;
  }
}

int CodeModel::columnCount(const QModelIndex &parent) const {
  if (auto line = d->LinePointerCast(parent)) {
    return static_cast<int>(line->columns.size());
  } else {
    return 1;
  }
}

QVariant CodeModel::data(const QModelIndex &index, int role) const {

  QVariant value;

  // We're dealing with the root node.
  if (!index.isValid()) {
    if (role == ICodeModel::ModelStateRole) {
      value.setValue(int(d->model_state));
    }
    return value;
  }

  // We're dealing with a line of data.
  if (auto line = d->LinePointerCast(index)) {

    if (line->number) {
      if (role == Qt::DisplayRole) {
        value.setValue(QString::number(line->number));

      } else if (role == ICodeModel::LineNumberRole) {
        value.setValue(line->number);
      }
    }

  // We're dealing with a column of data. Specifically, a token, or a fragment
  // of a token.
  } else if (auto col = d->ColumnPointerCast(index)) {
    switch (role) {
      case Qt::DisplayRole:
        value.setValue(col->data);
        break;
      case ICodeModel::TokenCategoryRole:
        value.setValue(col->category);
        break;
      case ICodeModel::TokenIdRole:
        value.setValue(d->tokens.tokens[col->token_index].id().Pack());
        break;
      case ICodeModel::LineNumberRole:
        value = index.parent().data(ICodeModel::LineNumberRole);
        break;
      case ICodeModel::RelatedEntityIdRole:
      case ICodeModel::RealRelatedEntityIdRole:
        if (auto eid = d->tokens.tokens[col->token_index].related_entity_id().Pack();
            eid != kInvalidEntityId) {
          value.setValue(eid);
        }
        break;

      // TODO(pag): Consider removing this role. It would be better to have the
      //            taint browser go and ask the database for the statement
      //            containing an token id, or just a general entity id.
      case ICodeModel::EntityIdOfStmtContainingTokenRole:
        break;
    }
  }

  return value;
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
  if (d->future_result.isCanceled()) {
    d->model_state = ModelState::UpdateCancelled;
    return;
  }

  IDatabase::IndexedTokenRangeDataResult future_result =
      d->future_result.takeResult();

  if (!future_result.Succeeded()) {
    d->model_state = ModelState::UpdateFailed;
    return;
  }

  // Check if anything actually changed.
  IndexedTokenRangeData new_tokens = future_result.TakeValue();
  if (new_tokens.requested_id == d->tokens.requested_id &&
      new_tokens.response_id == d->tokens.response_id &&
      new_tokens.tokens == d->tokens.tokens)  {
    d->model_state = ModelState::Ready;
    return;
  }

  emit beginResetModel();
  d->last_line_cache = nullptr;
  d->last_column_cache = nullptr;
  d->tokens = std::move(new_tokens);
  d->model_state = ModelState::Ready;
  emit endResetModel();
}

void CodeModel::OnExpandMacros(const TokenTreeVisitor *visitor) {
  if (visitor) {
    d->macro_expansion_config = visitor;
  } else {
    d->macro_expansion_config = &(d->empty_macro_expansion_config);
  }

  if (!IsReady()) {
    return;
  }

  std::optional<TokenTree> tt = TokenTree::from(d->tokens.tokens);
  if (!tt) {
    return;
  }

  CancelRunningRequest();

  d->model_state = ModelState::UpdateInProgress;
  d->future_result = d->database->RequestExpandedTokenRangeData(
      d->tokens.requested_id, tt.value(), d->macro_expansion_config);
  d->future_watcher.setFuture(d->future_result);
}

}  // namespace mx::gui
