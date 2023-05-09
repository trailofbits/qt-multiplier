/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "InformationExplorer.h"

#include <multiplier/ui/IInformationExplorer.h>

namespace mx::gui {

IInformationExplorer *
IInformationExplorer::Create(IInformationExplorerModel *model, QWidget *parent,
                             IGlobalHighlighter *global_highlighter) {

  return new InformationExplorer(model, parent, global_highlighter);
}

}  // namespace mx::gui
