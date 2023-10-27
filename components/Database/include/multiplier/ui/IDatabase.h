// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/ui/EntityInformation.h>
#include <multiplier/ui/IndexedTokenRangeData.h>
#include <multiplier/ui/Result.h>
#include <multiplier/ui/RPCErrorCode.h>

#include <multiplier/Index.h>
#include <multiplier/Token.h>

#include <QString>
#include <QFuture>

#include <deque>
#include <variant>
#include <optional>
#include <unordered_set>

namespace mx {
class TokenTree;
class TokenTreeVisitor;
}  // namespace mx
namespace mx::gui {

//! A generic template that defines a batched data receiver
template <typename DataType>
class IBatchedDataTypeReceiver {
 public:
  //! Constructor
  IBatchedDataTypeReceiver() = default;

  //! Destructor
  virtual ~IBatchedDataTypeReceiver() = default;

  //! Disabled copy constructor
  IBatchedDataTypeReceiver(const IBatchedDataTypeReceiver &) = delete;

  //! Disabled copy assignment operator
  IBatchedDataTypeReceiver &
  operator=(const IBatchedDataTypeReceiver &) = delete;

  //! A single batch of data of type `DataType`
  using DataBatch = std::deque<DataType>;

  //! A callback used to receive batched data
  virtual void OnDataBatch(DataBatch data_batch) = 0;
};

//! The IDatabase class is responsible for all async operations
class IDatabase {
 public:
  //! A pointer to an IDatabase object
  using Ptr = std::unique_ptr<IDatabase>;

  //! Constructor
  IDatabase() = default;

  //! Destructor
  virtual ~IDatabase() = default;

  //! Creates a new instance of the IDatabase object
  static Ptr Create(const Index &index,
                    const FileLocationCache &file_location_cache);

  //! Get the canonical entity for `eid`.
  virtual QFuture<VariantEntity> RequestCanonicalEntity(RawEntityId eid) = 0;

  //! A data batch receiver for EntityInformationResult objects
  using RequestEntityInformationReceiver =
      IBatchedDataTypeReceiver<EntityInformation>;

  //! Requests detailed information about a particular entity given its ID.
  //! \return True in case of success, or false otherwise
  virtual QFuture<bool>
  RequestEntityInformation(RequestEntityInformationReceiver &receiver,
                           const RawEntityId &entity_id) = 0;

  //! The output of a file or fragment request
  using IndexedTokenRangeDataResult =
      Result<IndexedTokenRangeData, RPCErrorCode>;

  //! Requests the specified file / fragment.
  virtual QFuture<IndexedTokenRangeDataResult>
  RequestIndexedTokenRangeData(RawEntityId entity_id,
                               const TokenTreeVisitor *vis) = 0;

  //! Requests the specified file
  virtual QFuture<IndexedTokenRangeDataResult>
  RequestExpandedTokenRangeData(RawEntityId entity_id, const TokenTree &tree,
                                const TokenTreeVisitor *vis) = 0;

  //! An optional name
  using OptionalName = std::optional<QString>;

  //! Starts a name resolution request for the given entity
  virtual QFuture<TokenRange> RequestEntityName(RawEntityId fragment_id) = 0;

  //! A list of related entities
  struct RelatedEntities final {
    //! The name of the entity used to perform the request
    QString name;

    //! The token containing the entity name (unreliable)
    TokenRange opt_name_tokens;

    //! Primary entity id
    RawEntityId primary_entity_id{};

    //! A list of related entity IDs
    std::unordered_set<RawEntityId> entity_id_list;
  };

  //! Either the result of the GetRelatedEntities request or an RPC error
  using RelatedEntitiesResult = Result<RelatedEntities, RPCErrorCode>;

  //! Requests a list of all the entities related to the given one
  virtual QFuture<RelatedEntitiesResult>
  GetRelatedEntities(RawEntityId entity_id) = 0;

  //! A data batch receiver for EntityQueryResult objects
  using QueryEntitiesReceiver = IBatchedDataTypeReceiver<Token>;

  //! String matching mode for QueryEntities
  enum class QueryEntitiesMode {
    ExactMatch,
    ContainingString,
  };

  //! Queries the internal index for all entities matching the search criteria
  //! \return True in case of success, or false otherwise
  virtual QFuture<bool> QueryEntities(QueryEntitiesReceiver &receiver,
                                      const QString &string,
                                      const QueryEntitiesMode &query_mode) = 0;

  //! Disabled copy constructor
  IDatabase(const IDatabase &) = delete;

  //! Disabled copy assignment operator
  IDatabase &operator=(const IDatabase &) = delete;
};

}  // namespace mx::gui
