/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "CodeModel.h"

#include <multiplier/Code.h>
#include <multiplier/ui/IRPC.h>

#include <QString>

#include <vector>
#include <iostream>

namespace mx::gui {

namespace {

enum class ModelState {
  UpdateInProgress,
  UpdateFailed,
  UpdateCancelled,
  Ready,
};

struct TokenColumn final {
  RawEntityId file_token_id;
  TokenCategory token_category;
  QString data;
};

using TokenRow = std::vector<TokenColumn>;
using TokenRowList = std::vector<TokenRow>;

}  // namespace

struct CodeModel::PrivateData final {
  PrivateData(const FileLocationCache &file_location_cache_, Index index_)
      : file_location_cache(file_location_cache_),
        index(std::move(index_)) {}

  const FileLocationCache &file_location_cache;
  Index index;

  IRPC::Ptr rpc;
  IRPC::FutureResult future_result;
  QFutureWatcher<IRPC::Result> future_watcher;

  ModelState model_state{ModelState::Ready};
  TokenRowList token_row_list;
};

CodeModel::~CodeModel() {}

const FileLocationCache &CodeModel::GetFileLocationCache() const {
  return d->file_location_cache;
}

Index &CodeModel::GetIndex() {
  return d->index;
}

void CodeModel::SetFile(RawEntityId file_id) {
  emit ModelAboutToBeReset();

  if (d->future_result.isRunning()) {
    d->future_result.cancel();
  }

  d->future_result = d->rpc->DownloadFile(file_id);
  d->future_watcher.setFuture(d->future_result);

  d->token_row_list.clear();

  d->model_state = ModelState::UpdateInProgress;

  emit ModelReset();
}

int CodeModel::RowCount() const {
  if (d->model_state != ModelState::Ready) {
    return 1;
  }

  return static_cast<int>(d->token_row_list.size());
}

int CodeModel::TokenCount(int row) const {
  if (d->model_state != ModelState::Ready) {
    return (row == 0 ? 1 : 0);
  }

  auto row_index = static_cast<std::size_t>(row);
  if (row_index >= d->token_row_list.size()) {
    return 0;
  }

  const auto &token_row = d->token_row_list.at(row_index);
  return static_cast<int>(token_row.size());
}

QVariant CodeModel::Data(const CodeModelIndex &index, int role) const {
  if (d->model_state != ModelState::Ready) {
    if (index.row != 0 || index.token_index != 0 || role != Qt::DisplayRole) {
      return QVariant();
    }

    switch (d->model_state) {
      case ModelState::UpdateInProgress: return tr("Update in progress...");
      case ModelState::UpdateCancelled: return tr("Update cancelled");
      case ModelState::UpdateFailed: return tr("Update failed");
      case ModelState::Ready: __builtin_unreachable();
    }
  }

  auto row_index = static_cast<std::size_t>(index.row);
  if (row_index >= d->token_row_list.size()) {
    return 0;
  }

  const auto &token_row = d->token_row_list.at(row_index);

  auto column_index = static_cast<std::size_t>(index.token_index);
  if (column_index >= token_row.size()) {
    return QVariant();
  }

  const auto &column = token_row.at(column_index);

  switch (role) {
    case Qt::DisplayRole: return column.data;

    case TokenCategoryRole:
      return static_cast<std::uint32_t>(column.token_category);

    default: return QVariant();
  }
}

CodeModel::CodeModel(const FileLocationCache &file_location_cache, Index index,
                     QObject *parent)
    : ICodeModel(parent),
      d(new PrivateData(file_location_cache, index)) {

  d->rpc = IRPC::Create(index, file_location_cache);
  connect(&d->future_watcher, &QFutureWatcher<IRPC::Result>::finished, this,
          &CodeModel::FutureResultStateChanged);
}

void CodeModel::FutureResultStateChanged() {
  d->token_row_list.clear();

  if (d->future_result.isCanceled()) {
    emit ModelAboutToBeReset();

    d->model_state = ModelState::UpdateCancelled;

    emit ModelReset();

    return;
  }

  auto future_result = d->future_result.takeResult();

  if (!future_result.Succeeded()) {
    emit ModelAboutToBeReset();

    d->model_state = ModelState::UpdateFailed;

    emit ModelReset();

    return;
  }

  emit ModelAboutToBeReset();

  auto indexed_token_range_data = future_result.TakeValue();
  auto token_count = indexed_token_range_data.start_of_token.size() - 1;

  TokenRow current_row;
  TokenColumn current_column;

  for (std::size_t i = 0; i < token_count; ++i) {
    auto token_start = indexed_token_range_data.start_of_token[i];
    auto token_end = indexed_token_range_data.start_of_token[i + 1];
    auto token_length = token_end - token_start;

    auto token = indexed_token_range_data.data.mid(token_start, token_length);

    current_column.file_token_id = indexed_token_range_data.file_token_ids[i];
    current_column.token_category =
        indexed_token_range_data.token_category_list[i];

    QString buffer;
    for (const auto &c : token) {
      if (c == QChar::LineSeparator) {
        auto current_column_copy = current_column;
        current_column_copy.data = std::move(buffer);
        buffer.clear();

        current_row.push_back(std::move(current_column_copy));
        d->token_row_list.push_back(std::move(current_row));
        current_row.clear();

        continue;
      }

      buffer.append(c);
    }

    if (!buffer.isEmpty()) {
      current_column.data = std::move(buffer);
      buffer.clear();

      current_row.push_back(std::move(current_column));
    }
  }

  if (!current_row.empty()) {
    d->token_row_list.push_back(std::move(current_row));
    current_row.clear();
  }

  d->model_state = ModelState::Ready;

  emit ModelReset();
}

}  // namespace mx::gui
