/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "TokenLabel.h"

#include <multiplier/ui/ITokenLabel.h>

namespace mx::gui {

ITokenLabel *ITokenLabel::Create(Token token, QWidget *parent) {
  return new TokenLabel(token, parent);
}

}  // namespace mx::gui
