/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "SearchWidget.h"

#include <multiplier/ui/ISearchWidget.h>

namespace mx::gui {

ISearchWidget *ISearchWidget::Create(Mode mode, QWidget *parent) {
  return new SearchWidget(mode, parent);
}

}  // namespace mx::gui
