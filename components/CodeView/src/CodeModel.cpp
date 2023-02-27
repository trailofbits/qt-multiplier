/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "CodeModel.h"

#include <multiplier/ui/Assert.h>
#include <multiplier/ui/IDatabase.h>
#include <multiplier/Entities/TokenCategory.h>

#include <QString>

#include <vector>

namespace mx::gui {

namespace {

struct TokenColumn final {
  RawEntityId token_id;
  RawEntityId related_entity_id;
  TokenCategory token_category;
  QString data;
  std::optional<std::uint64_t> opt_token_group_id;
};

using TokenColumnList = std::vector<TokenColumn>;

struct TokenRow final {
  std::size_t line_number{};
  TokenColumnList column_list;
};

using TokenRowList = std::vector<TokenRow>;

}  // namespace

struct CodeModel::PrivateData final {
  PrivateData(const FileLocationCache &file_location_cache_,
              const Index &index_)
      : file_location_cache(file_location_cache_),
        index(index_) {}

  FileLocationCache file_location_cache;
  Index index;

  IDatabase::Ptr database;
  IDatabase::FutureResult future_result;
  QFutureWatcher<IDatabase::Result> future_watcher;

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

void CodeModel::SetEntity(RawEntityId raw_id) {
  if (d->future_result.isRunning()) {
    d->future_result.cancel();
  }

  d->future_result = {};

  emit BeginResetModel();

  EntityId eid(raw_id);
  VariantId vid = eid.Unpack();

  if (std::holds_alternative<FileId>(vid)) {
    FileId fid = std::get<FileId>(vid);
    d->future_result = d->database->DownloadFile(fid);

  } else if (std::holds_alternative<FileTokenId>(vid)) {
    FileId fid(std::get<FileTokenId>(vid).file_id);
    d->future_result = d->database->DownloadFile(fid);

  } else if (std::optional<FragmentId> frag_id = FragmentId::from(eid)) {
    FragmentId fid = frag_id.value();
    d->future_result = d->database->DownloadFragment(fid);

  } else {
    emit EndResetModel(ModelState::UpdateFailed);
    return;
  }

  d->future_watcher.setFuture(d->future_result);
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
  return static_cast<int>(token_row.column_list.size());
}

QVariant CodeModel::Data(const CodeModelIndex &index, int role) const {
  if (d->model_state != ModelState::Ready) {
    if (index.row != 0 || index.token_index != 0) {
      return QVariant();
    }

    if (role == LineNumberRole || role == TokenRawEntityIdRole ||
        role == TokenRelatedEntityIdRole) {
      return 1;

    } else if (role == TokenCategoryRole) {
      return static_cast<std::uint32_t>(TokenCategory::UNKNOWN);

    } else if (role == Qt::DisplayRole) {
      switch (d->model_state) {
        case ModelState::UpdateInProgress:
          return tr("// The token request has started");

        case ModelState::UpdateCancelled:
          return tr("// The token request has been cancelled");

        case ModelState::UpdateFailed:
          return tr("// The token request has failed");

        case ModelState::Ready: __builtin_unreachable();
      }

    } else {
      return QVariant();
    }
  }

  auto row_index = static_cast<std::size_t>(index.row);
  if (row_index >= d->token_row_list.size()) {
    return 0;
  }

  const auto &token_row = d->token_row_list.at(row_index);

  auto column_index = static_cast<std::size_t>(index.token_index);
  if (column_index >= token_row.column_list.size()) {
    return QVariant();
  }

  const auto &column = token_row.column_list.at(column_index);

  switch (role) {
    case Qt::DisplayRole: return column.data;

    case TokenCategoryRole:
      return static_cast<std::uint32_t>(column.token_category);

    case TokenRawEntityIdRole:
      return static_cast<std::uint64_t>(column.token_id);

    case LineNumberRole:
      return static_cast<std::uint64_t>(token_row.line_number);

    case TokenGroupIdRole:
      if (column.opt_token_group_id.has_value()) {
        return column.opt_token_group_id.value();
      } else {
        return QVariant();
      }

    case TokenRelatedEntityIdRole:
      if (column.related_entity_id == kInvalidEntityId) {
        return QVariant();
      } else {
        return static_cast<std::uint64_t>(column.related_entity_id);
      }

    default: return QVariant();
  }
}

CodeModel::CodeModel(const FileLocationCache &file_location_cache,
                     const Index &index, QObject *parent)
    : ICodeModel(parent),
      d(new PrivateData(file_location_cache, index)) {

  d->database = IDatabase::Create(index, file_location_cache);
  connect(&d->future_watcher, &QFutureWatcher<IDatabase::Result>::finished,
          this, &CodeModel::FutureResultStateChanged);

  connect(this, &CodeModel::BeginResetModel, this,
          &CodeModel::OnBeginResetModel);
  connect(this, &CodeModel::EndResetModel, this, &CodeModel::OnEndResetModel);
}

void CodeModel::OnBeginResetModel() {
  d->model_state = ModelState::UpdateInProgress;
  d->token_row_list.clear();
}

void CodeModel::OnEndResetModel(const ModelState &model_state) {
  d->model_state = model_state;
  emit ModelReset();
}

void CodeModel::FutureResultStateChanged() {
  if (d->future_result.isCanceled()) {
    emit EndResetModel(ModelState::UpdateCancelled);
    return;
  }

  auto future_result = d->future_result.takeResult();
  if (!std::holds_alternative<IndexedTokenRangeData>(future_result)) {
    emit EndResetModel(ModelState::UpdateFailed);
    return;
  }

  IndexedTokenRangeData indexed_token_range_data =
      std::move(std::get<IndexedTokenRangeData>(future_result));

  auto token_count = indexed_token_range_data.start_of_token.size() - 1;

  TokenRow row;
  TokenColumn col;

  for (std::size_t i = 0; i < token_count; ++i) {
    auto token_start = indexed_token_range_data.start_of_token[i];
    auto token_end = indexed_token_range_data.start_of_token[i + 1];
    auto token_length = token_end - token_start;

    col.token_id = indexed_token_range_data.token_ids[i];
    col.related_entity_id = indexed_token_range_data.related_entity_ids[i];
    col.token_category = indexed_token_range_data.token_categories[i];

    auto frag_id_index = indexed_token_range_data.fragment_id_index[i];
    if (auto frag_id = indexed_token_range_data.fragment_ids[frag_id_index];
        frag_id != kInvalidEntityId) {
      col.opt_token_group_id.emplace(frag_id);
    }

    row.line_number = indexed_token_range_data.line_number[i];

    qsizetype line_number_adjust = 0;
    if (indexed_token_range_data.data[token_end - 1] == QChar::LineSeparator) {
      line_number_adjust = 1;
    }

    col.data = indexed_token_range_data.data.mid(
        token_start, token_length - line_number_adjust);
    row.column_list.push_back(std::move(col));

    if (line_number_adjust) {
      d->token_row_list.push_back(std::move(row));
    }
  }

  // Flush the last row.
  if (!row.column_list.empty()) {
    d->token_row_list.push_back(std::move(row));
  }

  emit EndResetModel(ModelState::Ready);
}

}  // namespace mx::gui
