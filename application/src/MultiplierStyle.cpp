// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "MultiplierStyle.h"

#include <QTabBar>

namespace mx::gui {

int MultiplierStyle::styleHint(
    StyleHint hint, const QStyleOption *option,
    const QWidget *widget, QStyleHintReturn *return_data) const {
  switch (hint) {
    case QStyle::SH_DockWidget_ButtonsHaveFrame:
      return 1;
    case QStyle::SH_TabBar_CloseButtonPosition:
      return QTabBar::ButtonPosition::LeftSide;
    default:
      return this->QProxyStyle::styleHint(hint, option, widget, return_data);
  }
};

}  // namespace mx::gui
