/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "IndexView.h"

#include <multiplier/ui/IIndexView.h>

namespace mx::gui {

IIndexView *IIndexView::Create(IFileTreeModel *model, QWidget *parent) {
  return new IndexView(model, parent);
}

}  // namespace mx::gui
