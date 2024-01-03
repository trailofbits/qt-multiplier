/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/GUI/Interfaces/IThemeProxy.h>
#include <QColor>
#include <unordered_map>

namespace mx::gui {

class HighlightThemeProxy Q_DECL_FINAL : public IThemeProxy {
  Q_OBJECT

 public:
  std::unordered_map<RawEntityId, std::pair<QColor, QColor>> color_map;

  virtual ~HighlightThemeProxy(void);

  ITheme::ColorAndStyle TokenColorAndStyle(
      ITheme::ColorAndStyle theme_color_and_style,
      const Token &token) const Q_DECL_FINAL;

  std::optional<QColor> EntityBackgroundColor(
      std::optional<QColor> theme_color,
      const VariantEntity &entity) const Q_DECL_FINAL;
};

}  // namespace mx::gui
