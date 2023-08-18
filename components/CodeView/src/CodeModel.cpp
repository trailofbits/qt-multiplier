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

  //
  // Compare the token ranges, and make a list of all the
  // token indexes that have changed
  //

  const auto &old_token_range = old_tokens.tokens;
  std::size_t old_token_index{};

  const auto &new_token_range = d->tokens.tokens;
  std::size_t new_token_index{};

  std::vector<std::size_t> removed_token_index_list;
  std::vector<std::size_t> added_token_index_list;

  for (;;) {
    if (old_token_index >= new_token_range.size()) {
      // We lost some tokens at the end of the document
      for (auto i = old_token_index; i < old_token_range.size(); ++i) {
        removed_token_index_list.push_back(i);
      }

      break;

    } else if (new_token_index >= new_token_range.size()) {
      // We have new tokens at the end of the document
      for (auto i = new_token_index; i < new_token_range.size(); ++i) {
        added_token_index_list.push_back(i);
      }

      break;
    }

    const auto &old_token = old_token_range[old_token_index];
    const auto &new_token = new_token_range[new_token_index];

    if (old_token.id() == new_token.id()) {
      ++old_token_index;
      ++new_token_index;
      continue;
    }

    // There is a difference here; attempt to realign the new token
    // range
    std::optional<std::size_t> opt_next_new_token_index;

    for (std::size_t i{new_token_index}; i < new_token_range.size(); ++i) {
      const auto &next_new_token = new_token_range[i];
      if (old_token.id() == next_new_token.id()) {
        opt_next_new_token_index = i;
        break;
      }
    }

    // Realign the token ranges
    if (opt_next_new_token_index.has_value()) {
      const auto &next_new_token_range_offset =
          opt_next_new_token_index.value();

      for (std::size_t i{new_token_index}; i < next_new_token_range_offset;
           ++i) {
        added_token_index_list.push_back(i);
      }

      ++old_token_index;
      new_token_index = next_new_token_range_offset + 1;

      continue;
    }

    // We could not realign the token ranges. Mark the current item as
    // removed and then try again
    removed_token_index_list.push_back(old_token_index);
    added_token_index_list.push_back(new_token_index);

    ++old_token_index;
    ++new_token_index;
  }

  // Next, we have to convert the token indexes we have generated
  // to lines
  auto L_createLineIndex = [](const IndexedTokenRangeData &tokens)
      -> std::unordered_map<std::size_t, std::size_t> {
    std::unordered_map<std::size_t, std::size_t> line_index;

    const auto &line_list = tokens.lines;
    for (std::size_t i{}; i < line_list.size(); ++i) {
      const auto &current_line = line_list[i];

      for (const auto &column : current_line.columns) {
        auto token_index = static_cast<std::size_t>(column.token_index);
        line_index.insert({token_index, i});
      }
    }

    return line_index;
  };

  auto L_gatherLineNumbers =
      [&L_createLineIndex](
          const IndexedTokenRangeData &indexed_token_range_data,
          const std::vector<std::size_t> &token_range_index_list)
      -> std::vector<std::size_t> {
    auto token_to_line_map = L_createLineIndex(indexed_token_range_data);

    std::vector<std::size_t> line_list;

    for (const auto &token_index : token_range_index_list) {
      auto line_number = token_to_line_map[token_index];

      const auto &token_data =
          indexed_token_range_data.tokens[token_index].data();
      if (token_data.empty() || line_number == 0) {
        // This needs further debugging. We are getting empty tokens in the
        // `TokenRange tokens` field of the IndexedTokenRangeData struct we
        // have received.
        //
        // Additionally, we may be getting newlines injected into the
        // tokens, which breaks the 1:1 line mapping we are currently
        // expecting here in the model and in the code view.
        //
        // Since the code view stores data per-line with block-relative
        // cursor positions, this would break the mapping.
        std::cerr
            << __FILE__ << "@" << __LINE__
            << ": Found a post-expansion difference at line 0. This is likely a bug\n";

        continue;
      }

      line_list.push_back(line_number);
    }

    return line_list;
  };

  auto removed_line_list =
      L_gatherLineNumbers(old_tokens, removed_token_index_list);

  auto added_line_list = L_gatherLineNumbers(d->tokens, added_token_index_list);

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
