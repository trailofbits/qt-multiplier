/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "TreeExplorer.h"

#include <multiplier/ui/ITreeExplorer.h>

namespace mx::gui {

ITreeExplorer *ITreeExplorer::Create(ITreeExplorerModel *model,
                                     IGlobalHighlighter *global_highlighter,
                                     QWidget *parent) {

  return new TreeExplorer(model, global_highlighter, parent);
}

}  // namespace mx::gui
