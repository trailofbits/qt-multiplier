/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/ui/ICodeView2.h>

#include "CodeView2.h"

namespace mx {

ICodeView2 *ICodeView2::Create(ICodeModel *model, QWidget *parent) {
  return new CodeView2(model, parent);
}

}  // namespace mx
