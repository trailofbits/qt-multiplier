/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/GUI/Interfaces/IGeneratedItem.h>

#include <multiplier/Index.h>

namespace mx::gui {

IGeneratedItem::~IGeneratedItem(void) {}

// Returns the entity ID aliased by this entity, or `VariantEntity`. This
// is a means of communicating equivalence of rows in terms of their
// child sets, but not necessarily in terms of their `Data`.
RawEntityId IGeneratedItem::AliasedEntityId(void) const {
  return kInvalidEntityId;
}

}  // namespace mx::gui
