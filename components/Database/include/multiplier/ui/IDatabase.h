// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/ui/RPCErrorCode.h>
#include <multiplier/ui/IndexedTokenRangeData.h>

#include <multiplier/Index.h>
#include <pasta/Util/Result.h>

#include <QFutureWatcher>
#include <QString>

#include <variant>
#include <vector>
#include <optional>

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
  using DataBatch = std::vector<DataType>;

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

  //! The output of a file or fragment request
  using IndexedTokenRangeDataResult =
      pasta::Result<IndexedTokenRangeData, RPCErrorCode>;

  //! Request type for RequestIndexedTokenRangeData
  enum class IndexedTokenRangeDataRequestType {
    Fragment,
    File,
  };

  //! Requests the specified file
  virtual QFuture<IndexedTokenRangeDataResult> RequestIndexedTokenRangeData(
      const RawEntityId &entity_id,
      const IndexedTokenRangeDataRequestType &request_type) = 0;

  //! An optional name
  using OptionalName = std::optional<QString>;

  //! Starts a name resolution request for the given entity
  virtual QFuture<OptionalName>
  RequestEntityName(const RawEntityId &fragment_id) = 0;

  //!
  struct EntityQueryResult final {
    //!
    Fragment fragment;

    //!
    std::optional<File> opt_file;

    //! The entity name
    std::string name;

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
