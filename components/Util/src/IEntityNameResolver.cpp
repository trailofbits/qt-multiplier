// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "EntityNameResolver.h"

#include <multiplier/ui/IEntityNameResolver.h>

namespace mx::gui {

IEntityNameResolver *IEntityNameResolver::Create(Index index,
                                                 const RawEntityId &entity_id) {
  return new EntityNameResolver(std::move(index), entity_id);
}

}  // namespace mx::gui
