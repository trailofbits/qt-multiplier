/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/GUI/Managers/ThemeManager.h>

namespace mx::gui {

IThemeProxy::~IThemeProxy(void) {}

void IThemeProxy::UninstallFromOwningManager(void) {
  emit Uninstall(this);
}

QFont IThemeProxy::Font(QFont theme_font) const {
  return theme_font;
}

QColor IThemeProxy::IconColor(QColor theme_color, ITheme::IconStyle) const {
  return theme_color;
}

QColor IThemeProxy::GutterForegroundColor(QColor theme_color) const {
  return theme_color;
}

QColor IThemeProxy::GutterBackgroundColor(QColor theme_color) const {
  return theme_color;
}

QColor IThemeProxy::DefaultForegroundColor(QColor theme_color) const {
  return theme_color;
}

QColor IThemeProxy::DefaultBackgroundColor(QColor theme_color) const {
  return theme_color;
}

QColor IThemeProxy::CurrentLineBackgroundColor(QColor theme_color) const {
  return theme_color;
}

QColor IThemeProxy::CurrentEntityBackgroundColor(
    QColor theme_color, const VariantEntity &) const {
  return theme_color;
}

ITheme::ColorAndStyle IThemeProxy::TokenColorAndStyle(
    ITheme::ColorAndStyle theme_color_and_style, const Token &) const {
  return theme_color_and_style;
}

std::optional<QColor> IThemeProxy::EntityBackgroundColor(
    std::optional<QColor> theme_color, const VariantEntity &) const {
  return theme_color;
}

}  // namespace mx::gui
