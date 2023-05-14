// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "Database.h"

#include <requests/GetIndexedTokenRangeData.h>
#include <requests/GetEntityName.h>
#include <requests/GetEntityInformation.h>
#include <requests/GetEntityList.h>
#include <requests/GetRelatedEntities.h>
#include <requests/GetTokenTree.h>

#include <QThreadPool>
#include <QtConcurrent>
#include <thread>

namespace mx::gui {

struct Database::PrivateData {
  PrivateData(const Index &index_,
              const FileLocationCache &file_location_cache_)
      : index(index_),
        file_location_cache(file_location_cache_) {}

  const Index index;
  const FileLocationCache file_location_cache;
};

Database::~Database() {}

QFuture<IDatabase::TokenTreeResult>
Database::RequestTokenTree(RawEntityId entity_id) {
  return QtConcurrent::run(QThreadPool::globalInstance(), GetTokenTree,
                           d->index, entity_id);
}

QFuture<IDatabase::EntityInformationResult>
Database::RequestEntityInformation(RawEntityId entity_id) {
  return QtConcurrent::run(QThreadPool::globalInstance(), GetEntityInformation,
                           d->index, d->file_location_cache, entity_id);
}

QFuture<IDatabase::IndexedTokenRangeDataResult>
Database::RequestIndexedTokenRangeData(
    TokenTree tree, std::unique_ptr<TokenTreeVisitor> visitor) {
  return QtConcurrent::run(QThreadPool::globalInstance(),
                           GetIndexedTokenRangeData, d->index,
                           d->file_location_cache, tree, std::move(visitor));
}

QFuture<OptionalName> Database::RequestEntityName(RawEntityId fragment_id) {
  return QtConcurrent::run(QThreadPool::globalInstance(), GetEntityName,
                           d->index, fragment_id);
}

QFuture<IDatabase::EntityIDList>
Database::GetRelatedEntities(RawEntityId entity_id) {
  return QtConcurrent::run(QThreadPool::globalInstance(),
                           mx::gui::GetRelatedEntities, d->index, entity_id);
}

QFuture<bool> Database::QueryEntities(QueryEntitiesReceiver &receiver,
                                      const QString &name,
                                      const bool &exact_name) {
  return QtConcurrent::run(QThreadPool::globalInstance(), GetEntityList,
                           d->index, &receiver, name, exact_name);
}

Database::Database(const Index &index,
                   const FileLocationCache &file_location_cache)
    : d(new PrivateData(index, file_location_cache)) {}

}  // namespace mx::gui
