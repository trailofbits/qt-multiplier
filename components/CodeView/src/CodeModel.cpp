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
#include <set>
#include <sstream>
#include <fstream>
#include <iostream>

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

  const IndexedTokenRangeData::Line *LinePointerCast(const QModelIndex &index);

  const IndexedTokenRangeData::Column *
  ColumnPointerCast(const QModelIndex &index);

  IDatabase::Ptr database;
  QFuture<IDatabase::IndexedTokenRangeDataResult> future_result;
  QFutureWatcher<IDatabase::IndexedTokenRangeDataResult> future_watcher;
};

const IndexedTokenRangeData::Line *
CodeModel::PrivateData::LinePointerCast(const QModelIndex &model_index) {
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

const IndexedTokenRangeData::Column *
CodeModel::PrivateData::ColumnPointerCast(const QModelIndex &model_index) {
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
      case Qt::DisplayRole: value.setValue(col->data); break;
      case ICodeModel::TokenCategoryRole: value.setValue(col->category); break;
      case ICodeModel::TokenIdRole:
        value.setValue(d->tokens.tokens[col->token_index].id().Pack());
        break;
      case ICodeModel::LineNumberRole:
        value = index.parent().data(ICodeModel::LineNumberRole);
        break;
      case ICodeModel::RelatedEntityIdRole:
      case ICodeModel::RealRelatedEntityIdRole:
        if (auto eid =
                d->tokens.tokens[col->token_index].related_entity_id().Pack();
            eid != kInvalidEntityId) {
          value.setValue(eid);
        }
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

  IndexedTokenRangeData new_tokens = future_result.TakeValue();
  if (new_tokens.requested_id != d->tokens.requested_id ||
      new_tokens.response_id != d->tokens.response_id) {

    emit beginResetModel();
    d->last_line_cache = nullptr;
    d->last_column_cache = nullptr;
    d->tokens = std::move(new_tokens);
    d->model_state = ModelState::Ready;
    emit endResetModel();

    return;
  }

  if (new_tokens.tokens == d->tokens.tokens) {
    d->model_state = ModelState::Ready;
    return;
  }

  auto old_tokens = std::move(d->tokens);
  d->tokens = std::move(new_tokens);

  d->last_line_cache = nullptr;
  d->last_column_cache = nullptr;
  d->model_state = ModelState::Ready;

  // Compare the old lines with the new lines to determine what
  // has changed
  std::vector<std::size_t> removed_line_list;
  const auto &old_line_list = old_tokens.lines;
  std::size_t old_line_index{};

  std::vector<std::size_t> added_line_list;
  const auto &new_line_list = d->tokens.lines;
  std::size_t new_line_index{};

  for (;;) {
    // Make sure we can access both the old and new lines at the
    // current indexes
    if (old_line_index >= new_line_list.size()) {
      // We lost some lines at the end of the document
      for (auto i{old_line_index}; i < old_line_list.size(); ++i) {
        removed_line_list.push_back(i);
      }

      break;

    } else if (new_line_index >= old_line_list.size()) {
      // We have brand new lines at the end of the document
      for (auto i{new_line_index}; i < new_line_list.size(); ++i) {
        added_line_list.push_back(i);
      }

      break;
    }

    // Compare the current lines
    const auto &old_line = old_line_list[old_line_index];
    const auto &new_line = new_line_list[new_line_index];

    if (old_line.hash == new_line.hash) {
      ++old_line_index;
      ++new_line_index;

      continue;
    }

    // The current lines are different; look for the missing data
    // in the new line list
    std::optional<std::size_t> opt_next_new_line_index{std::nullopt};

    for (auto i{new_line_index}; i < new_line_list.size(); ++i) {
      const auto &next_new_line = new_line_list[i];

      if (old_line.hash == next_new_line.hash) {
        opt_next_new_line_index = i;
        break;
      }
    }

    // Check whether we have found a matching new line we can realign to
    if (opt_next_new_line_index.has_value()) {
      // We have found the missing line further in the new line list.
      // Mark everything between the old line and the new line as
      // added
      const auto &next_new_line_index = opt_next_new_line_index.value();

      for (auto i{new_line_index}; i < next_new_line_index; ++i) {
        added_line_list.push_back(i);
      }

      ++old_line_index;
      new_line_index = next_new_line_index + 1;

      continue;
    }

    // We could not find a way to realign the streams. Mark the old line as
    // removed and the new line as added
    added_line_list.push_back(new_line_index);
    removed_line_list.push_back(old_line_index);

    ++old_line_index;
    ++new_line_index;
  }

  // Now convert the line numbers to ranges
  using Range = std::pair<std::size_t, std::size_t>;
  using RangeList = std::vector<Range>;

  auto L_convertToRanges = [](std::vector<std::size_t> line_list) -> RangeList {
    std::sort(line_list.begin(), line_list.end());

    auto it = std::unique(line_list.begin(), line_list.end());
    line_list.erase(it, line_list.end());

    auto line_list_it = line_list.begin();
    std::optional<Range> opt_range;

    std::vector<std::pair<std::size_t, std::size_t>> range_list;

    while (line_list_it != line_list.end()) {
      const auto &line = *line_list_it;

      if (opt_range.has_value()) {
        auto &range = opt_range.value();

        if (range.second + 1 == line) {
          ++range.second;
          ++line_list_it;
          continue;

        } else {
          range_list.push_back(std::move(range));
          opt_range = std::nullopt;
          continue;
        }
      }

      opt_range = std::make_pair(line, line);
      ++line_list_it;
    }

    if (opt_range.has_value()) {
      auto &range = opt_range.value();
      range_list.push_back(std::move(range));
    }

    return range_list;
  };

  // Send the row removals in reverse order so we don't have to rebase
  // the ranges
  auto removed_line_range_list = L_convertToRanges(removed_line_list);
  std::sort(removed_line_range_list.begin(), removed_line_range_list.end(),
            [](const Range &lhs, const Range &rhs) -> bool {
              return lhs.first > rhs.first;
            });

  auto added_line_range_list = L_convertToRanges(added_line_list);

  for (const auto &removed_line_range : removed_line_range_list) {
    auto first_line = static_cast<int>(removed_line_range.first);
    auto last_line = static_cast<int>(removed_line_range.second);

    emit beginRemoveRows(QModelIndex(), first_line, last_line);
    emit endRemoveRows();
  }

  // Finally, send the row insertions
  for (const auto &added_line_range : added_line_range_list) {
    auto first_line = static_cast<int>(added_line_range.first);
    auto last_line = static_cast<int>(added_line_range.second);

    emit beginInsertRows(QModelIndex(), first_line, last_line);
    emit endInsertRows();
  }
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
