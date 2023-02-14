/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/IDatabase.h>

#include <QPromise>

#include <variant>

namespace mx::gui {

enum class DownloadRequestType {
  FileTokens,
  FragmentTokens,
};

struct SingleEntityRequest final {
  DownloadRequestType download_request_type;
  RawEntityId entity_id;
};

using Request = std::variant<SingleEntityRequest>;
using IndexedTokenRangeDataResult = std::variant<IndexedTokenRangeData, RPCErrorCode>;
using IndexedTokenRangeDataResultPromise =
    QPromise<IndexedTokenRangeDataResult>;

void CreateIndexedTokenRangeData(
    IndexedTokenRangeDataResultPromise &result_promise, const Index &index,
    const FileLocationCache &file_location_cache, const Request &request);

}  // namespace mx::gui
