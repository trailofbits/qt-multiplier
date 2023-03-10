// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/ui/RPCErrorCode.h>
#include <multiplier/ui/DatabaseFuture.h>
#include <multiplier/ui/IndexedTokenRangeData.h>

#include <multiplier/Index.h>
#include <pasta/Util/Result.h>

#include <QFuture>
#include <QFutureWatcher>
#include <QString>

#include <variant>
#include <vector>
#include <optional>

namespace mx::gui {

class IDatabase {
 public:
  using Ptr = std::unique_ptr<IDatabase>;

  IDatabase() = default;
  virtual ~IDatabase() = default;

  static Ptr Create(const Index &index,
                    const FileLocationCache &file_location_cache);

  using FileResult = pasta::Result<IndexedTokenRangeData, RPCErrorCode>;
  virtual QFuture<FileResult> DownloadFile(const RawEntityId &file_id) = 0;
  virtual QFuture<FileResult>
  DownloadFragment(const RawEntityId &fragment_id) = 0;

  virtual QFuture<std::optional<QString>>
  GetEntityName(const RawEntityId &fragment_id) = 0;

  IDatabase(const IDatabase &) = delete;
  IDatabase &operator=(const IDatabase &) = delete;
};

}  // namespace mx::gui
