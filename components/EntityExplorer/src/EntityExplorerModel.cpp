/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "EntityExplorerModel.h"

#include <QFutureWatcher>
#include <QTimer>

namespace mx::gui {

namespace {

const int kInitialUpdateTimer{500};
const int kUpdateTimer{1000};

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

  std::vector<IDatabase::QueryEntitiesReceiver::DataBatch> data_batch_queue;
  std::mutex data_batch_mutex;

  QTimer update_timer;
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

  } else if (role == EntityExplorerModel::TokenIdRole) {
    value.setValue(static_cast<qulonglong>(row->entity_id));
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

void EntityExplorerModel::Search(const QString &name,
                                 const SearchMode &search_mode) {
  CancelSearch();

  auto query_mode = search_mode == SearchMode::ExactMatch
                        ? IDatabase::QueryEntitiesMode::ExactMatch
                        : IDatabase::QueryEntitiesMode::ContainingString;

  d->request_status_future =
      d->database->QueryEntities(*this, name, query_mode);

  d->future_watcher.setFuture(d->request_status_future);

  d->update_timer.start(kInitialUpdateTimer);

  emit SearchStarted();
}

void EntityExplorerModel::CancelSearch() {
  if (d->request_status_future.isRunning()) {
    d->request_status_future.cancel();
    d->request_status_future.waitForFinished();

    d->request_status_future = {};
  }

  d->update_timer.stop();

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

  connect(&d->update_timer, &QTimer::timeout, this,
          &EntityExplorerModel::ProcessDataBatchQueue);
}

void EntityExplorerModel::OnDataBatch(
    IDatabase::QueryEntitiesReceiver::DataBatch data_batch) {

  std::lock_guard<std::mutex> lock(d->data_batch_mutex);
  d->data_batch_queue.push_back(std::move(data_batch));
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

  // Use a stable sort to maintain order from Multiplier API, which orders by
  // entity IDs. This puts definitions before declarations.
  std::stable_sort(d->row_list.begin(), d->row_list.end(),
                   EntityQueryResultCmp{});

  if (d->sorting_method == SortingMethod::Descending) {
    std::reverse(d->row_list.begin(), d->row_list.end());
  }
}

void EntityExplorerModel::ProcessDataBatchQueue() {
  std::vector<IDatabase::QueryEntitiesReceiver::DataBatch> data_batch_queue;

  {
    std::lock_guard<std::mutex> lock(d->data_batch_mutex);
    data_batch_queue = std::move(d->data_batch_queue);

    d->data_batch_queue.clear();
  }

  for (auto &data_batch : data_batch_queue) {
    TokenCategorySet token_category_set;
    if (d->opt_token_category_set.has_value()) {
      token_category_set = d->opt_token_category_set.value();
    }

    // If our sorting method is reverse, then put the entities back in their
    // original order, so that when we call `SortRows` we get a properly
    // maintained stable sort.
    if (d->sorting_method == SortingMethod::Descending) {
      std::reverse(d->row_list.begin(), d->row_list.end());
    }

    for (auto &data_batch_entity : data_batch) {
      const IDatabase::EntityQueryResult &entity =
          d->results.emplace_back(std::move(data_batch_entity));

      if (!EntityIncludedInTokenCategorySet(entity,
                                            d->opt_token_category_set)) {
        continue;
      }

      if (!RegexMatchesEntityName(entity, d->opt_regex)) {
        continue;
      }

      d->row_list.push_back(&entity);
    }
  }

  SortRows();

  emit beginResetModel();
  emit endResetModel();

  if (d->request_status_future.isRunning()) {
    d->update_timer.start(kUpdateTimer);

  } else {
    d->update_timer.stop();
  }
}

}  // namespace mx::gui
