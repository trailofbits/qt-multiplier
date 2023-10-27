/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/ui/ITreeGenerator.h>

#include <multiplier/Types.h>

namespace mx::gui {

// Generate the root / top-level items for the tree.
//
// NOTE(pag): These are `shared_ptr`s so that implementations have the
//            flexibility of having tree items extend the lifetime of
//            tree generator itself.
gap::generator<std::shared_ptr<ITreeItem>> ITreeGenerator::Roots(
    const std::shared_ptr<ITreeGenerator> &self) {
  return Children(self, kInvalidEntityId);
}

}  // namespace mx::gui
