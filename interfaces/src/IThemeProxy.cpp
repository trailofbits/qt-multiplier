/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/GUI/Interfaces/IThemeProxy.h>

namespace mx::gui {

IThemeProxy::~IThemeProxy(void) {}

void IThemeProxy::UninstallFromOwningManager(void) {
  emit Uninstall(this);
}

QFont IThemeProxy::Font(const ITheme &, QFont theme_font) const {
  return theme_font;
}

QColor IThemeProxy::CursorColor(const ITheme &, QColor theme_color) const {
  return theme_color;
}

QColor IThemeProxy::SelectionColor(const ITheme &, QColor theme_color) const {
  return theme_color;
}

QColor IThemeProxy::IconColor(const ITheme &, QColor theme_color,
                              ITheme::IconStyle) const {
  return theme_color;
}

QColor IThemeProxy::GutterForegroundColor(
    const ITheme &, QColor theme_color) const {
  return theme_color;
}

QColor IThemeProxy::GutterBackgroundColor(
    const ITheme &, QColor theme_color) const {
  return theme_color;
}

QColor IThemeProxy::DefaultForegroundColor(
    const ITheme &, QColor theme_color) const {
  return theme_color;
}

QColor IThemeProxy::DefaultBackgroundColor(
    const ITheme &, QColor theme_color) const {
  return theme_color;
}

QColor IThemeProxy::CurrentLineBackgroundColor(
    const ITheme &, QColor theme_color) const {
  return theme_color;
}

QColor IThemeProxy::CurrentEntityBackgroundColor(
    const ITheme &, QColor theme_color, const VariantEntity &) const {
  return theme_color;
}

ITheme::ColorAndStyle IThemeProxy::TokenColorAndStyle(
    const ITheme &, ITheme::ColorAndStyle theme_color_and_style,
    const Token &) const {
  return theme_color_and_style;
}

std::optional<QColor> IThemeProxy::EntityBackgroundColor(
    const ITheme &, std::optional<QColor> theme_color,
    const VariantEntity &) const {
  return theme_color;
}

void IThemeProxy::EmitThemeProxyChanged(void) {
  emit ThemeProxyChanged();
}

}  // namespace mx::gui
