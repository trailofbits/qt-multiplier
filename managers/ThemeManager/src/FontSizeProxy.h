// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Interfaces/IThemeProxy.h>

namespace mx::gui {

// Private theme proxy that adjusts font size by a delta. Lazily installed
// by ThemeManager when the user first requests a font size change.
class FontSizeProxy final : public IThemeProxy {
  Q_OBJECT

  int delta{0};

 public:
  FontSizeProxy(void) = default;

  QFont Font(const ITheme &theme, QFont theme_font) const override;

  void Increment(void);
  void Decrement(void);
  void Reset(void);
};

}  // namespace mx::gui
