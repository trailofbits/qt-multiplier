/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/GUI/Interfaces/IThemeProxy.h>

namespace mx::gui {

//! A theme proxy that adjusts the code font size by a delta relative to
//! the underlying theme's font size. Install via ThemeManager::AddProxy().
class FontSizeProxy final : public IThemeProxy {
  Q_OBJECT

  int delta{0};

 public:
  FontSizeProxy(void) = default;

  QFont Font(const ITheme &theme, QFont theme_font) const override;

  int Delta(void) const;
  void Increment(void);
  void Decrement(void);
  void Reset(void);
};

}  // namespace mx::gui
