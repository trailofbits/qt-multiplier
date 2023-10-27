/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/IDatabase.h>

#include <multiplier/Index.h>

#include <QPromise>

namespace mx::gui {

void GetEntityName(QPromise<TokenRange> &entity_name_promise,
                   const Index &index, RawEntityId entity_id);

}  // namespace mx::gui
