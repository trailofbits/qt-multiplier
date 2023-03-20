/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "EntityExplorerModel.h"

namespace mx::gui {

struct EntityExplorerModel::PrivateData final {
  Index index;
  FileLocationCache file_location_cache;

  IDatabase::Ptr database;
  QFuture<bool> request_status_future;
  QFutureWatcher<bool> future_watcher;

  IDatabase::QueryEntitiesReceiver::DataBatch row_list;
};

EntityExplorerModel::~EntityExplorerModel() {
  CancelSearch();
}

int EntityExplorerModel::rowCount(const QModelIndex &) const {
  return static_cast<int>(d->row_list.size());
}

QVariant EntityExplorerModel::data(const QModelIndex &index, int role) const {
  QVariant value;
  if (!index.isValid() || role != Qt::DisplayRole) {
    return value;
  }

  auto row_it = d->row_list.begin();
  row_it = std::next(row_it, index.row());

  if (row_it != d->row_list.end()) {
    const auto &row = *row_it;
    value.setValue(QString::fromStdString(row.name));
  }

  return value;
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
          &EntityExplorerModel::RequestFutureStatusChanged);

  connect(&d->future_watcher, &QFutureWatcher<QFuture<bool>>::finished, this,
          &EntityExplorerModel::SearchFinished);
}

void EntityExplorerModel::OnDataBatch(
    IDatabase::QueryEntitiesReceiver::DataBatch data_batch) {

  auto first_row = static_cast<int>(d->row_list.size());
  auto last_row = first_row + static_cast<int>(data_batch.size());

  emit beginInsertRows(QModelIndex(), first_row, last_row);

  d->row_list.insert(d->row_list.end(),
                     std::make_move_iterator(data_batch.begin()),
                     std::make_move_iterator(data_batch.end()));

  emit endInsertRows();
}

void EntityExplorerModel::RequestFutureStatusChanged() {}

}  // namespace mx::gui
