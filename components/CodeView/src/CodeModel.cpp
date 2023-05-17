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

}  // namespace

struct CodeModel::PrivateData final {
  PrivateData(const FileLocationCache &file_location_cache_,
              const Index &index_)
      : file_location_cache(file_location_cache_),
        index(index_) {}

  FileLocationCache file_location_cache;
  Index index;

  ModelState model_state{ModelState::Ready};
  std::optional<RawEntityId> opt_entity_id;

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

  if (model_index.row() != 0) {
    return nullptr;
  }

  auto tok = reinterpret_cast<const IndexedTokenRangeData::Column *>(ptr);
  auto num_lines = tokens.lines.size();
  if (tok->line_index >= num_lines) {  // Make unsafe :-P
    return nullptr;
  }

  const IndexedTokenRangeData::Line &line = tokens.lines[tok->line_index];
  last_line_cache = &line;

  unsigned col = static_cast<unsigned>(model_index.column());
  if (auto num_cols = line.columns.size(); col < num_cols) {
    auto first = line.columns.data();
    auto last = &(first[num_cols - 1u]);
    if (first <= tok && tok <= last) {
      last_column_cache = tok;
      return tok;
    }
  }
  return nullptr;
}

CodeModel::~CodeModel() {
  CancelRunningRequest();
}

std::optional<RawEntityId> CodeModel::GetEntity(void) const {
  return d->opt_entity_id;
}

void CodeModel::SetEntity(RawEntityId raw_id) {
  CancelRunningRequest();

  emit beginResetModel();

  EntityId eid(raw_id);
  if (std::optional<FragmentId> frag_id = FragmentId::from(eid)) {
    raw_id = EntityId(frag_id.value()).Pack();
  }

  if (d->opt_entity_id.has_value() && d->opt_entity_id.value() == raw_id) {
    emit endResetModel();
    return;
  }

  d->opt_entity_id = std::nullopt;
  d->future_result = d->database->RequestTokenTree(raw_id).then(
      QtFuture::Launch::Inherit,
      [this] (IDatabase::TokenTreeResult tt_res) -> QFuture<IDatabase::IndexedTokenRangeDataResult> {
        if (!tt_res.Succeeded()) {
          QFutureInterface<IDatabase::IndexedTokenRangeDataResult> fi;
          IDatabase::IndexedTokenRangeDataResult err = tt_res.TakeError();
          fi.reportFinished(&err);
          return QFuture<IDatabase::IndexedTokenRangeDataResult>(&fi);
        } else {
          return d->database->RequestIndexedTokenRangeData(
              tt_res.TakeValue(), std::make_unique<TokenTreeVisitor>());
        }
      }).unwrap();

  d->model_state = ModelState::UpdateInProgress;
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
    if (auto line = d->LinePointerCast(parent)) {
      if (auto col_index = static_cast<unsigned>(column);
          col_index < line->columns.size()) {
        return createIndex(row, column, &(line->columns[col_index]));
      }
    }
  } else {
    if (auto row_index = static_cast<unsigned>(row);
        row_index < d->tokens.lines.size()) {
      return createIndex(row, column, &(d->tokens.lines[row_index]));
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
  if (!index.isValid()) {
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
  emit beginResetModel();

  if (d->future_result.isCanceled()) {
    d->model_state = ModelState::UpdateCancelled;
    emit endResetModel();
    return;
  }

  IDatabase::IndexedTokenRangeDataResult future_result =
      d->future_result.takeResult();

  if (!future_result.Succeeded()) {
    d->model_state = ModelState::UpdateFailed;
    emit endResetModel();
    return;
  }

  d->last_line_cache = nullptr;
  d->last_column_cache = nullptr;
  d->tokens = future_result.TakeValue();
  d->model_state = ModelState::Ready;
  emit endResetModel();
}

}  // namespace mx::gui
