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

  //! Disabled copy constructor
  IDatabase(const IDatabase &) = delete;

  //! Disabled copy assignment operator
  IDatabase &operator=(const IDatabase &) = delete;
};

}  // namespace mx::gui
