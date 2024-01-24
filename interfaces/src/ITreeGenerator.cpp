/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/GUI/Interfaces/ITreeGenerator.h>

#include <multiplier/Index.h>

namespace mx::gui {

// Return the initialize expansion depth (defaults to `2`).
unsigned ITreeGenerator::InitialExpansionDepth(void) const {
  return 2u;
}

// Return the index of the default sort column, or `-1` to disable sorting.
// The default implementation of this method returns `0`.
int ITreeGenerator::SortColumn(void) const {
  return 0u;
}

// Return `true` to enable `IGeneratedItem::Entity`- and
// `IGeneratedItem::AliasedEntity`-based deduplication. The default
// implementation of this method returns `true`.
bool ITreeGenerator::EnableDeduplication(void) const {
  return true;
}

}  // namespace mx::gui
