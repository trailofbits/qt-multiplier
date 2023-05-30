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

QFuture<IDatabase::EntityInformationResult>
Database::RequestEntityInformation(RawEntityId entity_id) {
  return QtConcurrent::run(QThreadPool::globalInstance(), GetEntityInformation,
                           d->index, d->file_location_cache, entity_id);
}

//! \copybrief IDatabase::RequestCanonicalEntity
QFuture<VariantEntity> Database::RequestCanonicalEntity(RawEntityId entity_id) {
  return QtConcurrent::run(
      QThreadPool::globalInstance(),
      [](const Index &index, RawEntityId eid) -> VariantEntity {
        VariantEntity ent = index.entity(eid);
        if (std::holds_alternative<Decl>(ent)) {
          return std::get<Decl>(ent).canonical_declaration();
        } else {
          return ent;
        }
      },
      d->index, entity_id);
}

QFuture<IDatabase::IndexedTokenRangeDataResult>
Database::RequestIndexedTokenRangeData(RawEntityId entity_id) {
  return QtConcurrent::run(QThreadPool::globalInstance(),
                           GetIndexedTokenRangeData, d->index,
                           d->file_location_cache, entity_id);
}

QFuture<OptionalName> Database::RequestEntityName(RawEntityId fragment_id) {
  return QtConcurrent::run(QThreadPool::globalInstance(), GetEntityName,
                           d->index, fragment_id);
}

QFuture<IDatabase::RelatedEntitiesResult>
Database::GetRelatedEntities(RawEntityId entity_id) {
  return QtConcurrent::run(QThreadPool::globalInstance(),
                           mx::gui::GetRelatedEntities, d->index, entity_id);
}

QFuture<bool> Database::QueryEntities(QueryEntitiesReceiver &receiver,
                                      const QString &string,
                                      const QueryEntitiesMode &query_mode) {
  return QtConcurrent::run(QThreadPool::globalInstance(), GetEntityList,
                           d->index, &receiver, string, query_mode);
}

Database::Database(const Index &index,
                   const FileLocationCache &file_location_cache)
    : d(new PrivateData(index, file_location_cache)) {}

}  // namespace mx::gui
