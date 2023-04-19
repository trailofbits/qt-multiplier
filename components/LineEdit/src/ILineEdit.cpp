/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "LineEdit.h"

#include <multiplier/ui/ILineEdit.h>

namespace mx::gui {

ILineEdit *ILineEdit::Create(QWidget *parent) {
  return new LineEdit(parent);
}

}  // namespace mx::gui
