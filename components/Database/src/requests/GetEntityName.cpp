/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "GetEntityName.h"
#include "Utils.h"

namespace mx::gui {

void GetEntityName(QPromise<OptionalName> &entity_name_promise,
                   const Index &index, RawEntityId entity_id) {

  auto opt_name = LookupEntityName(index, entity_id);
  entity_name_promise.addResult(opt_name);
}

}  // namespace mx::gui
