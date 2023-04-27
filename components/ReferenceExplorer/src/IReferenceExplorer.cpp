/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "ReferenceExplorer.h"

#include <multiplier/ui/IReferenceExplorer.h>

namespace mx::gui {

IReferenceExplorer *
IReferenceExplorer::Create(IReferenceExplorerModel *model,
                           const IReferenceExplorer::Mode &mode,
                           QWidget *parent) {

  return new ReferenceExplorer(model, mode, parent);
}

}  // namespace mx::gui
