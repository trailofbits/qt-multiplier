/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/GUI/CodeViewTheme.h>

#include <multiplier/Frontend/TokenCategory.h>

#include "DefaultCodeViewThemes.h"

namespace mx::gui {

CodeViewTheme GetCodeViewTheme(const bool &dark) {
  return dark ? kDefaultDarkCodeViewTheme : kDefaultLightCodeViewTheme;
}

QColor CodeViewTheme::ForegroundColor(TokenCategory category) const {
  if (auto it = token_foreground_color_map.find(category);
      it != token_foreground_color_map.end()) {
    return it->second;
  } else {
    return default_foreground_color;
  }
}

QColor CodeViewTheme::BackgroundColor(TokenCategory category) const {
  if (auto it = token_background_color_map.find(category);
      it != token_background_color_map.end()) {
    return it->second;
  } else {
    return default_background_color;
  }
}

CodeViewTheme::Style CodeViewTheme::TextStyle(TokenCategory category) const {
  if (auto it = token_style_map.find(category); it != token_style_map.end()) {
    return it->second;
  } else {
    return {};
  }
}

QColor CodeViewTheme::ForegroundColor(const QVariant &category) const {
  bool ok = true;
  if (!category.isValid()) {
    return default_foreground_color;
  } else if (auto as_uint = category.toUInt(&ok);
             ok && as_uint < NumEnumerators(TokenCategory{})) {
    return ForegroundColor(static_cast<TokenCategory>(as_uint));
  } else {
    return {};
  }
}

QColor CodeViewTheme::BackgroundColor(const QVariant &category) const {
  bool ok = true;
  if (!category.isValid()) {
    return default_background_color;
  } else if (auto as_uint = category.toUInt(&ok);
             ok && as_uint < NumEnumerators(TokenCategory{})) {
    return BackgroundColor(static_cast<TokenCategory>(as_uint));
  } else {
    return default_background_color;
  }
}

CodeViewTheme::Style CodeViewTheme::TextStyle(const QVariant &category) const {
  bool ok = true;
  if (!category.isValid()) {
    return {};
  } else if (auto as_uint = category.toUInt(&ok);
             ok && as_uint < NumEnumerators(TokenCategory{})) {
    return TextStyle(static_cast<TokenCategory>(as_uint));
  } else {
    return {};
  }
}

}  // namespace mx::gui
