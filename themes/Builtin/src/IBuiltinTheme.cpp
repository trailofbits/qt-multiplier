/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "IBuiltinTheme.h"

#include <multiplier/GUI/Managers/MediaManager.h>

namespace mx::gui {

IBuiltinTheme::~IBuiltinTheme(void) {}

IBuiltinTheme::IBuiltinTheme(MediaManager &media, QString name_, QString id_,
                             QPalette palette_, const ThemeData &data_)
    : font(media.Font("Source Code Pro")),
      id(std::move(id_)),
      name(std::move(name_)),
      palette(std::move(palette_)),
      data(data_) {
  font.setStyleHint(QFont::TypeWriter);        
}

QString IBuiltinTheme::Name(void) const {
  return name;
}

QString IBuiltinTheme::Id(void) const {
  return id;
}

QFont IBuiltinTheme::Font(void) const {
  return font;
}

QColor IBuiltinTheme::GutterForegroundColor(void) const {
  return data.default_gutter_foreground;
}

QColor IBuiltinTheme::GutterBackgroundColor(void) const {
  return data.default_gutter_background;
}

QColor IBuiltinTheme::DefaultForegroundColor(void) const {
  return data.default_foreground_color;
}

QColor IBuiltinTheme::DefaultBackgroundColor(void) const {
  return data.default_background_color;
}

QColor IBuiltinTheme::CurrentLineBackgroundColor(void) const {
  return data.selected_line_background_color;
}

QColor IBuiltinTheme::CurrentEntityBackgroundColor(void) const {
  return data.highlighted_entity_background_color;
}

ColorAndStyle IBuiltinTheme::TokenColorAndStyle(const Token &token) const {
  return data.token_styles[static_cast<unsigned>(token.category())];
}

}  // namespace mx::gui
