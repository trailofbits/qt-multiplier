/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "GetRelatedEntities.h"

namespace mx::gui {

void GetRelatedEntities(
    QPromise<IDatabase::EntityIDList> &related_entities_promise,
    const Index &index, RawEntityId entity_id) {

  static_cast<void>(index);

  IDatabase::EntityIDList entity_list{entity_id};
  related_entities_promise.addResult(std::move(entity_list));
}

}  // namespace mx::gui
