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
IReferenceExplorer::Create(IReferenceExplorerModel *model, QWidget *parent,
                           IGlobalHighlighter *global_highlighter) {

  return new ReferenceExplorer(model, parent, global_highlighter);
}

}  // namespace mx::gui
