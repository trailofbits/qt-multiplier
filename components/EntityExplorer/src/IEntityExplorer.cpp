/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "EntityExplorer.h"

#include <multiplier/ui/IEntityExplorer.h>

namespace mx::gui {

IEntityExplorer *IEntityExplorer::Create(IEntityExplorerModel *model,
                                         QWidget *parent) {

  try {
    return new EntityExplorer(model, parent);

  } catch (const std::bad_alloc &) {
    return nullptr;
  }
}

}  // namespace mx::gui
