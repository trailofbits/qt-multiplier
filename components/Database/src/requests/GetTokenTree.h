/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/IDatabase.h>

#include <QPromise>

namespace mx::gui {

void GetTokenTree(
    QPromise<IDatabase::TokenTreeResult> &token_tree_promise,
    const Index &index, RawEntityId entity_id);

}  // namespace mx::gui
