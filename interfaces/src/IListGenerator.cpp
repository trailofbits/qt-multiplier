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

// Return the initialize expansion depth (defaults to `1`).
unsigned IListGenerator::InitialExpansionDepth(void) const {
  return 1u;
}

gap::generator<IGeneratedItemPtr> IListGenerator::Children(
    const ITreeGeneratorPtr &, const VariantEntity &) {
  co_return;
}

}  // namespace mx::gui
