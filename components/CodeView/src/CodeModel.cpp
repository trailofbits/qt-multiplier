/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "CodeModel.h"

#include <multiplier/Code.h>
#include <multiplier/DownloadCodeThread.h>

#include <QDebug>
#include <QThreadPool>
#include <atomic>
#include <iostream>
#include <vector>

namespace mx::gui {

namespace {

enum class ModelState {
  UpdateInProgress,
  UpdateFailed,
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

  std::atomic_uint64_t counter{1000};
  ModelState state{ModelState::Ready};
  std::unique_ptr<Code> code;
  TokenRowList token_row_list;
};

CodeModel::~CodeModel() {}

const FileLocationCache &CodeModel::GetFileLocationCache() const {
  return d->file_location_cache;
}

Index &CodeModel::GetIndex() {
  return d->index;
}

void CodeModel::SetFile(const Index &index, RawEntityId file_id) {
  emit ModelAboutToBeReset();

  d->state = ModelState::UpdateInProgress;

  // TODO(alessandro): This will not play nice if there are other components
  // that can end up using the same ID. Convert this to cancellable requests
  // and remove the need to use IDs
  auto prev_counter = d->counter.fetch_add(1u);  // Go to the next version.

  auto downloader = DownloadCodeThread::CreateFileDownloader(
      index, CodeTheme::DefaultTheme(), GetFileLocationCache(),
      prev_counter + 1u, file_id);

  connect(downloader, &DownloadCodeThread::DownloadFailed, this,
          &CodeModel::OnDownloadFailed);

  connect(downloader, &DownloadCodeThread::RenderCode, this,
          &CodeModel::OnDownloadSucceeded);

  QThreadPool::globalInstance()->start(downloader);

  emit ModelReset();
}

int CodeModel::RowCount() const {
  if (d->state == ModelState::UpdateInProgress) {
    return 1;
  }

  return static_cast<int>(d->token_row_list.size());
}

int CodeModel::TokenCount(int row) const {
  if (d->state == ModelState::UpdateInProgress ||
      d->state == ModelState::UpdateFailed) {
    if (row == 0) {
      return 1;
    }

    return 0;
  }

  auto row_index = static_cast<std::size_t>(row);
  if (row_index >= d->token_row_list.size()) {
    return 0;
  }

  const auto &token_row = d->token_row_list.at(row_index);
  return static_cast<int>(token_row.size());
}

QVariant CodeModel::Data(const CodeModelIndex &index, int role) const {
  if (d->state == ModelState::UpdateInProgress ||
      d->state == ModelState::UpdateFailed) {
    if (index.row != 0 || index.token_index != 0 || role != Qt::DisplayRole) {
      return QVariant();
    }

    if (d->state == ModelState::UpdateInProgress) {
      return tr("Update in progress...");
    } else {
      return tr("Update failed");
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
  case Qt::DisplayRole:
    return column.data;

  case TokenCategoryRole:
    return static_cast<std::uint32_t>(column.token_category);

  default:
    return QVariant();
  }
}

CodeModel::CodeModel(const FileLocationCache &file_location_cache, Index index,
                     QObject *parent)
    : ICodeModel(parent),
      d(new PrivateData(file_location_cache, index)) {}

void CodeModel::OnDownloadFailed() {
  emit ModelAboutToBeReset();

  // TODO(alessandro): We have no idea which request (numer) has failed. Trying
  // to display a message here is racy. Remove the IDs and use a single re-usable
  // thread
  d->state = ModelState::UpdateFailed;
  d->token_row_list.clear();
  d->code.reset();

  emit ModelReset();
}

void CodeModel::OnDownloadSucceeded(void *code, uint64_t counter) {
  emit ModelAboutToBeReset();

  // TODO(alessandro): Remove `counter` usage and add support for
  // aborting pending requests
  if (d->counter.load() != counter) {
    d->state = ModelState::UpdateFailed;
    emit ModelReset();
    return;
  }

  d->state = ModelState::Ready;

  {
    // TODO(alessandro): Avoid reinterpret_cast
    std::unique_ptr<Code> temp_code(reinterpret_cast<Code *>(code));
    d->code.swap(temp_code);
  }

  d->token_row_list.clear();

  auto token_count = d->code->start_of_token.size() - 1;

  TokenRow current_row;
  TokenColumn current_column;

  for (std::size_t i = 0; i < token_count; ++i) {
    auto token_start = d->code->start_of_token[i];
    auto token_end = d->code->start_of_token[i + 1];
    auto token_length = token_end - token_start;

    auto token = d->code->data.mid(token_start, token_length);

    current_column.file_token_id = d->code->file_token_ids[i];
    current_column.token_category = d->code->token_category_list[i];

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

  emit ModelReset();
}

}  // namespace mx::gui
