/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/GUI/Interfaces/ITreeGenerator.h>

#include <multiplier/Index.h>

namespace mx::gui {

// Generate the root / top-level items for the tree.
//
// NOTE(pag): These are `shared_ptr`s so that implementations have the
//            flexibility of having tree items extend the lifetime of
//            tree generator itself.
gap::generator<IGeneratedItemPtr> ITreeGenerator::Roots(
    const ITreeGeneratorPtr &self) {
  return Children(self, NotAnEntity{});
}

}  // namespace mx::gui
