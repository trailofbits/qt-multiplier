/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/GUI/Themes/FontSizeProxy.h>

#include <algorithm>

namespace mx::gui {

QFont FontSizeProxy::Font(const ITheme &, QFont theme_font) const {
  int size = std::clamp(theme_font.pointSize() + delta, 6, 48);
  theme_font.setPointSize(size);
  return theme_font;
}

int FontSizeProxy::Delta(void) const {
  return delta;
}

void FontSizeProxy::Increment(void) {
  ++delta;
  EmitThemeProxyChanged();
}

void FontSizeProxy::Decrement(void) {
  --delta;
  EmitThemeProxyChanged();
}

void FontSizeProxy::Reset(void) {
  if (delta != 0) {
    delta = 0;
    EmitThemeProxyChanged();
  }
}

}  // namespace mx::gui
