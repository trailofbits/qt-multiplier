/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/GUI/Managers/ThemeManager.h>

namespace mx::gui {

ITheme::~ITheme(void) {}

std::optional<QColor> ITheme::EntityBackgroundColor(
    const VariantEntity &) const {
  return std::nullopt;
}

}  // namespace mx::gui
