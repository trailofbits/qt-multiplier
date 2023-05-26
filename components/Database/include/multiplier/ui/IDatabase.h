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

#include <QString>
#include <QFuture>

#include <deque>
#include <variant>
#include <optional>
#include <unordered_set>

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

  //! A slot used to receive batched data
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

  //! The output of an entity information request
  using EntityInformationResult = Result<EntityInformation, RPCErrorCode>;

  //! Requests detailed information about a particular entity given its ID.
  virtual QFuture<EntityInformationResult>
  RequestEntityInformation(RawEntityId entity_id) = 0;

  //! The output of a file or fragment request
  using IndexedTokenRangeDataResult =
      Result<IndexedTokenRangeData, RPCErrorCode>;

  //! Request type for RequestIndexedTokenRangeData
  enum class IndexedTokenRangeDataRequestType {
    Fragment,
    File,
  };

  //! Requests the specified file
  virtual QFuture<IndexedTokenRangeDataResult>
  RequestIndexedTokenRangeData(RawEntityId entity_id) = 0;

  //! An optional name
  using OptionalName = std::optional<QString>;

  //! Starts a name resolution request for the given entity
  virtual QFuture<OptionalName> RequestEntityName(RawEntityId fragment_id) = 0;

  //! A list of related entities
  struct RelatedEntities final {
    //! The name of the entity used to perform the request
    QString name;

    //! A list of related entity IDs
    std::unordered_set<RawEntityId> entity_id_list;
  };

  //! Either the result of the GetRelatedEntities request or an RPC error
  using RelatedEntitiesResult = Result<RelatedEntities, RPCErrorCode>;

  //! Requests a list of all the entities related to the given one
  virtual QFuture<RelatedEntitiesResult>
  GetRelatedEntities(RawEntityId entity_id) = 0;

  //! A single entity query result
  struct EntityQueryResult final {
    //! The fragment containing this token
    Fragment fragment;

    //! The file containing this token
    std::optional<File> opt_file;

    //! The entity name
    Token name_token;

    //! The entity data
    std::variant<NamedDecl, DefineMacroDirective> data;
  };

  //! A data batch receiver for EntityQueryResult objects
  using QueryEntitiesReceiver = IBatchedDataTypeReceiver<EntityQueryResult>;

  //! Queries the internal index for all entities named like `name`
  //! \return True in case of success, or false otherwise
  virtual QFuture<bool> QueryEntities(QueryEntitiesReceiver &receiver,
                                      const QString &name,
                                      const bool &exact_name) = 0;

  //! Disabled copy constructor
  IDatabase(const IDatabase &) = delete;

  //! Disabled copy assignment operator
  IDatabase &operator=(const IDatabase &) = delete;
};

}  // namespace mx::gui
