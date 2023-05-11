/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "ProjectExplorer.h"

#include <multiplier/ui/IProjectExplorer.h>

namespace mx::gui {

IProjectExplorer *IProjectExplorer::Create(IFileTreeModel *model,
                                           QWidget *parent) {
  return new ProjectExplorer(model, parent);
}

}  // namespace mx::gui
