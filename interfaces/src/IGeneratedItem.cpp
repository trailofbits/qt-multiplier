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

VariantEntity IGeneratedItem::AliasedEntity(void) const {
  return NotAnEntity{};
}

}  // namespace mx::gui
