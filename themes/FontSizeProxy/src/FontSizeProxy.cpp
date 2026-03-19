/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/GUI/Themes/FontSizeProxy.h>

#include <algorithm>

namespace mx::gui {

FontSizeProxy::FontSizeProxy(int initial_size)
    : point_size(initial_size) {}

QFont FontSizeProxy::Font(const ITheme &, QFont theme_font) const {
  theme_font.setPointSize(point_size);
  return theme_font;
}

int FontSizeProxy::PointSize(void) const {
  return point_size;
}

void FontSizeProxy::SetPointSize(int size) {
  size = std::clamp(size, 6, 48);
  if (size != point_size) {
    point_size = size;
    EmitThemeProxyChanged();
  }
}

}  // namespace mx::gui
