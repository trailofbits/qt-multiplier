// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "Database.h"

#include <requests/GetIndexedTokenRangeData.h>
#include <requests/GetEntityName.h>
#include <requests/GetEntityList.h>

#include <QThreadPool>
#include <QtConcurrent>
#include <thread>

namespace mx::gui {

struct Database::PrivateData {
  PrivateData(const Index &index_,
              const FileLocationCache &file_location_cache_)
      : index(std::move(index_)),
        file_location_cache(file_location_cache_) {}

  const Index index;
  const FileLocationCache file_location_cache;

  QThreadPool thread_pool;
};

Database::~Database() {}

QFuture<IDatabase::IndexedTokenRangeDataResult>
Database::RequestIndexedTokenRangeData(
    const RawEntityId &entity_id,
    const IndexedTokenRangeDataRequestType &request_type) {

  return QtConcurrent::run(&d->thread_pool, GetIndexedTokenRangeData, d->index,
                           d->file_location_cache, entity_id, request_type);
}

QFuture<OptionalName>
Database::RequestEntityName(const RawEntityId &fragment_id) {
  return QtConcurrent::run(&d->thread_pool, GetEntityName, d->index,
                           fragment_id);
}

QFuture<bool> Database::QueryEntities(QueryEntitiesReceiver &receiver,
                                      const QString &name,
                                      const bool &exact_name) {
  return QtConcurrent::run(&d->thread_pool, GetEntityList, d->index, &receiver,
                           name, exact_name);
}

Database::Database(const Index &index,
                   const FileLocationCache &file_location_cache)
    : d(new PrivateData(index, file_location_cache)) {

  d->thread_pool.setMaxThreadCount(
      static_cast<int>(std::thread::hardware_concurrency()));
}

}  // namespace mx::gui
