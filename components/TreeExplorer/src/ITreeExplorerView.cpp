/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "TreeExplorerView.h"

#include <multiplier/ui/ITreeExplorerView.h>

namespace mx::gui {

ITreeExplorerView *
ITreeExplorerView::Create(IGeneratorModel *model,
                          IGlobalHighlighter *global_highlighter,
                          QWidget *parent) {

  return new TreeExplorerView(model, global_highlighter, parent);
}

}  // namespace mx::gui
