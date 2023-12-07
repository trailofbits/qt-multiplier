/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/IDatabase.h>
#include <multiplier/Frontend/TokenTree.h>

#include <QPromise>

namespace mx::gui {

void GetExpandedTokenRangeData(
    QPromise<IDatabase::IndexedTokenRangeDataResult> &result_promise,
    const Index &, const FileLocationCache &file_location_cache,
    RawEntityId entity_id, TokenTree tree, const TokenTreeVisitor *vis);

void GetIndexedTokenRangeData(
    QPromise<IDatabase::IndexedTokenRangeDataResult> &result_promise,
    const Index &index, const FileLocationCache &file_location_cache,
    RawEntityId entity_id, const TokenTreeVisitor *vis);

}  // namespace mx::gui
