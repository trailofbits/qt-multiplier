/*
  Copyright (c) 2024-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/GUI/Interfaces/IListGenerator.h>

#include <multiplier/Index.h>

namespace mx::gui {

// Return the number of columns of data. This is always 1.
int IListGenerator::NumColumns(void) const {
  return 1;
}

// Return the index of the default sort column. This always returns 0.
int IListGenerator::SortColumn(void) const {
  return 0;
}

// Return `true` to enable `IGeneratedItem::Entity`- and
// `IGeneratedItem::AliasedEntity`-based deduplication. This always returns
// `true`.
bool IListGenerator::EnableDeduplication(void) const {
  return true;
}

// Return the initialize expansion depth (defaults to `1`).
unsigned IListGenerator::InitialExpansionDepth(void) const {
  return 1u;
}

gap::generator<IGeneratedItemPtr> IListGenerator::Children(
    ITreeGeneratorPtr, IGeneratedItemPtr) {
  co_return;
}

}  // namespace mx::gui
