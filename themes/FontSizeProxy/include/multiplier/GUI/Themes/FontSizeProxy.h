/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/GUI/Interfaces/IThemeProxy.h>

namespace mx::gui {

//! A theme proxy that overrides the font size. Install this via
//! ThemeManager::AddProxy() and then call SetPointSize() to change
//! the code font size at runtime.
class FontSizeProxy final : public IThemeProxy {
  Q_OBJECT

  int point_size;

 public:
  explicit FontSizeProxy(int initial_size = 14);

  QFont Font(const ITheme &theme, QFont theme_font) const override;

  int PointSize(void) const;
  void SetPointSize(int size);
};

}  // namespace mx::gui
