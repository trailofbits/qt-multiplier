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

  std::atomic_uint64_t counter{0};
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
  emit beginResetModel();

  d->state = ModelState::UpdateInProgress;

  auto prev_counter = d->counter.fetch_add(1u);  // Go to the next version.

  auto downloader = DownloadCodeThread::CreateFileDownloader(
      index, CodeTheme::DefaultTheme(), GetFileLocationCache(),
      prev_counter + 1u, file_id);

  connect(downloader, &DownloadCodeThread::DownloadFailed, this,
          &CodeModel::OnDownloadFailed);

  connect(downloader, &DownloadCodeThread::RenderCode, this,
          &CodeModel::OnRenderCode);

  QThreadPool::globalInstance()->start(downloader);

  emit endResetModel();
}

int CodeModel::rowCount(const QModelIndex &parent) const {
  if (d->state == ModelState::UpdateInProgress) {
    return 1;
  }

  return 0;
}

int CodeModel::columnCount(const QModelIndex &parent) const {
  if (d->state == ModelState::UpdateInProgress) {
    return 1;
  }

  return 0;
}

QVariant CodeModel::data(const QModelIndex &index, int role) const {
  if (d->state == ModelState::UpdateInProgress) {
    if (index.row() != 0 || index.column() != 0 || role != Qt::DisplayRole) {
      return QVariant();
    }

    return QString(tr("Update in progress..."));
  }

  return QVariant();
}

CodeModel::CodeModel(const FileLocationCache &file_location_cache, Index index,
                     QObject *parent)
    : ICodeModel(parent),
      d(new PrivateData(file_location_cache, index)) {}

void CodeModel::OnDownloadFailed() {
  // TODO(alessandro): We have no idea which request has failed. Trying
  // to display a message here is racy
  d->state = ModelState::UpdateFailed;
  emit dataChanged(index(0, 0), index(0, 0));
}

void CodeModel::OnRenderCode(void *code, uint64_t counter) {
  // TODO(alessandro): Remove `counter` usage and add support for
  // aborting pending requests
  if (d->counter.load() != counter) {
    emit beginResetModel();
    d->state = ModelState::UpdateFailed;
    emit endResetModel();
    return;
  }

  emit beginResetModel();

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

    QString buffer;
    for (const auto &c : token) {
      if (c != '\n') {
        buffer.append(c);
        continue;
      }

      auto current_column_copy = current_column;
      current_column_copy.data = std::move(buffer);
      buffer.clear();

      current_row.push_back(std::move(current_column_copy));
      d->token_row_list.push_back(std::move(current_row));
      current_row.clear();
    }

    if (!buffer.isEmpty()) {
      current_column.data = std::move(buffer);
      buffer.clear();

      current_row.push_back(std::move(current_column));
    }
  }

  emit endResetModel();
}

}  // namespace mx::gui
