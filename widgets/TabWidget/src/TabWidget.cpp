/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/ui/TabWidget.h>

#include "TabBar.h"

namespace mx::gui {

TabWidget::~TabWidget(void) {}

TabWidget::TabWidget(QWidget *parent)
    : QTabWidget(parent) {
  setTabBar(new TabBar(this));
}

}  // namespace mx::gui
