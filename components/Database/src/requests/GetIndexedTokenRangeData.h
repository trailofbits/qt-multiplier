/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/IDatabase.h>

#include <QPromise>

namespace mx::gui {

void GetIndexedTokenRangeData(
    QPromise<IDatabase::IndexedTokenRangeDataResult> &result_promise,
    const Index &index, const FileLocationCache &file_location_cache,
    const RawEntityId &entity_id,
    const IDatabase::IndexedTokenRangeDataRequestType &request_type);

}  // namespace mx::gui
