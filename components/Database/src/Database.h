// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/ui/IDatabase.h>

namespace mx::gui {

//! The main implementation for the IDatabase interface
class Database final : public IDatabase {
 public:
  //! Destructor
  virtual ~Database() override;

  //! \copybrief IDatabase::RequestCanonicalEntity
  virtual QFuture<VariantEntity>
  RequestCanonicalEntity(RawEntityId eid) override;

  //! \copybrief IDatabase::RequestEntityInformation
  virtual QFuture<bool>
  RequestEntityInformation(RequestEntityInformationReceiver &receiver,
                           const RawEntityId &entity_id) override;

  //! \copybrief IDatabase::RequestIndexedTokenRangeData
  virtual QFuture<IndexedTokenRangeDataResult>
  RequestIndexedTokenRangeData(RawEntityId entity_id,
                               const TokenTreeVisitor *vis) override;

  //! \copybrief IDatabase::RequestExpandedTokenRangeData
  virtual QFuture<IndexedTokenRangeDataResult>
  RequestExpandedTokenRangeData(RawEntityId entity_id, const TokenTree &tree,
                                const TokenTreeVisitor *vis) override;

  //! \copybrief IDatabase::RequestEntityName
  virtual QFuture<Token> RequestEntityName(RawEntityId fragment_id) override;

  //! \copybrief IDatabase::GetRelatedEntities
  virtual QFuture<RelatedEntitiesResult>
  GetRelatedEntities(RawEntityId entity_id) override;

  //! \copybrief IDatabase::QueryEntities
  virtual QFuture<bool>
  QueryEntities(QueryEntitiesReceiver &receiver, const QString &string,
                const QueryEntitiesMode &query_mode) override;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Constructor
  Database(const Index &index, const FileLocationCache &file_location_cache);

  friend class IDatabase;
};

}  // namespace mx::gui
