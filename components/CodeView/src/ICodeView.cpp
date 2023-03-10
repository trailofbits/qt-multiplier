/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "CodeView.h"

#include <multiplier/ui/ICodeView.h>

namespace mx::gui {

ICodeView *ICodeView::Create(ICodeModel *model, QWidget *parent) {
  return new CodeView(model, parent);
}

}  // namespace mx::gui
