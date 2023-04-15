/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "EntityExplorerModel.h"

#include <set>
#include <unordered_map>

namespace mx::gui {

namespace {

struct EntityQueryResultCmp final {
  bool operator()(const IDatabase::EntityQueryResult *lhs,
                  const IDatabase::EntityQueryResult *rhs) const {

    const auto &lhs_string_view = lhs->name_token.data();
    const auto &rhs_string_view = rhs->name_token.data();

    return lhs_string_view < rhs_string_view;
  }
};

bool RegexMatchesEntityName(
    const IDatabase::EntityQueryResult &entity,
    const std::optional<QRegularExpression> &opt_regex) {

  if (!opt_regex.has_value()) {
    return true;
  }

  const auto &regex = opt_regex.value();

  auto string_view = entity.name_token.data();
  auto string_view_size = static_cast<qsizetype>(string_view.size());

  auto entity_name = QString::fromUtf8(string_view.data(), string_view_size);

  auto match = regex.match(entity_name);
  return match.hasMatch();
}

bool EntityIncludedInTokenCategorySet(
    const IDatabase::EntityQueryResult &entity,
    const std::optional<EntityExplorerModel::TokenCategorySet>
        &opt_token_category_set) {

  if (!opt_token_category_set.has_value()) {
    return true;
  }

  const auto &token_category_set = opt_token_category_set.value();
  return token_category_set.count(entity.name_token.category()) > 0;
}

}  // namespace

struct EntityExplorerModel::PrivateData final {
  Index index;
  FileLocationCache file_location_cache;

  IDatabase::Ptr database;
  QFuture<bool> request_status_future;
  QFutureWatcher<bool> future_watcher;

  std::deque<IDatabase::EntityQueryResult> results;

  std::optional<TokenCategorySet> opt_token_category_set{};
  SortingMethod sorting_method{SortingMethod::Ascending};
  std::optional<QRegularExpression> opt_regex;

  std::vector<const IDatabase::EntityQueryResult *> row_list;
};

EntityExplorerModel::~EntityExplorerModel() {
  CancelSearch();
}

void EntityExplorerModel::SetSortingMethod(
    const SortingMethod &sorting_method) {

  if (sorting_method != d->sorting_method) {
    emit beginResetModel();
    d->sorting_method = sorting_method;
    std::reverse(d->row_list.begin(), d->row_list.end());
    emit endResetModel();
  }
}

void EntityExplorerModel::SetFilterRegularExpression(
    const QRegularExpression &regex) {
  emit beginResetModel();

  d->opt_regex = regex;
  GenerateRows();
  SortRows();
  emit endResetModel();
}

void EntityExplorerModel::SetTokenCategoryFilter(
    const std::optional<TokenCategorySet> &opt_token_category_set) {

  emit beginResetModel();

  d->opt_token_category_set = opt_token_category_set;
  GenerateRows();
  SortRows();
  emit endResetModel();
}

int EntityExplorerModel::rowCount(const QModelIndex &) const {
  return static_cast<int>(d->row_list.size());
}

QVariant EntityExplorerModel::data(const QModelIndex &index, int role) const {
  QVariant value;
  if (!index.isValid()) {
    return value;
  }

  auto row_index = static_cast<unsigned>(index.row());
  if (row_index >= d->row_list.size()) {
    return value;
  }

  const IDatabase::EntityQueryResult *row = d->row_list[row_index];
  if (!row) {
    return value;  // Shouldn't happen.
  }

  if (role == Qt::DisplayRole) {
    std::string_view name = row->name_token.data();
    qsizetype name_len = static_cast<qsizetype>(name.size());
    value.setValue(QString::fromUtf8(name.data(), name_len));

  } else if (role == EntityExplorerModel::TokenRole) {
    value.setValue(row->name_token);
  }

  return value;
}

QModelIndex EntityExplorerModel::index(int row, int column,
                                       const QModelIndex &) const {
  if (row >= rowCount(QModelIndex()) || column != 0) {
    return QModelIndex();
  }

  return createIndex(row, column);
}

QModelIndex EntityExplorerModel::parent(const QModelIndex &) const {
  return QModelIndex();
}

int EntityExplorerModel::columnCount(const QModelIndex &) const {
  return 1;
}

void EntityExplorerModel::Search(const QString &name, const bool &exact_name) {
  CancelSearch();

  d->request_status_future =
      d->database->QueryEntities(*this, name, exact_name);

  d->future_watcher.setFuture(d->request_status_future);

  emit SearchStarted();
}

void EntityExplorerModel::CancelSearch() {
  if (d->request_status_future.isRunning()) {
    d->request_status_future.cancel();
    d->request_status_future.waitForFinished();

    d->request_status_future = {};
  }

  emit beginResetModel();

  d->results.clear();
  d->row_list.clear();

  emit endResetModel();
}

EntityExplorerModel::EntityExplorerModel(
    const Index &index, const FileLocationCache &file_location_cache,
    QObject *parent)
    : IEntityExplorerModel(parent),
      d(new PrivateData) {

  d->index = index;
  d->file_location_cache = file_location_cache;

  d->database = IDatabase::Create(index, file_location_cache);

  connect(&d->future_watcher, &QFutureWatcher<QFuture<bool>>::finished, this,
          &EntityExplorerModel::SearchFinished);
}

void EntityExplorerModel::OnDataBatch(
    IDatabase::QueryEntitiesReceiver::DataBatch data_batch) {

  emit beginResetModel();

  TokenCategorySet token_category_set;
  if (d->opt_token_category_set.has_value()) {
    token_category_set = d->opt_token_category_set.value();
  }

  for (auto &data_batch_entity : data_batch) {
    const IDatabase::EntityQueryResult &entity =
        d->results.emplace_back(std::move(data_batch_entity));

    if (!EntityIncludedInTokenCategorySet(entity, d->opt_token_category_set)) {
      continue;
    }

    if (!RegexMatchesEntityName(entity, d->opt_regex)) {
      continue;
    }

    d->row_list.push_back(&entity);
  }

  SortRows();

  emit endResetModel();
}

void EntityExplorerModel::GenerateRows(void) {
  d->row_list.clear();

  for (const IDatabase::EntityQueryResult &entity : d->results) {
    if (!EntityIncludedInTokenCategorySet(entity, d->opt_token_category_set)) {
      continue;
    }

    if (!RegexMatchesEntityName(entity, d->opt_regex)) {
      continue;
    }

    d->row_list.push_back(&entity);
  }
}

void EntityExplorerModel::SortRows(void) {
  std::sort(d->row_list.begin(), d->row_list.end(), EntityQueryResultCmp{});
  if (d->sorting_method == SortingMethod::Descending) {
    std::reverse(d->row_list.begin(), d->row_list.end());
  }
}

}  // namespace mx::gui
