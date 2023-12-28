/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QColor>
#include <QVariant>

#include <unordered_map>
#include <vector>

namespace mx {
enum class TokenKind : unsigned short;
enum class TokenCategory : unsigned char;
}  // namespace mx

namespace mx::gui {

//! This structure represents a theme for the ICodeView widget
struct CodeViewTheme final {
  struct Style final {
    bool bold{false};
    bool underline{false};
    bool strikeout{false};
    bool italic{false};
  };

  QString font_name;

  QColor selected_line_background_color;
  QColor highlighted_entity_background_color;

  QColor default_background_color;
  QColor default_foreground_color;

  QColor default_gutter_background;
  QColor default_gutter_foreground;

  std::unordered_map<TokenCategory, Style> token_style_map;
  std::unordered_map<TokenCategory, QColor> token_background_color_map;
  std::unordered_map<TokenCategory, QColor> token_foreground_color_map;

  std::vector<QColor> token_group_color_list;

  QColor ForegroundColor(TokenCategory category) const;
  QColor BackgroundColor(TokenCategory category) const;
  Style TextStyle(TokenCategory category) const;

  QColor ForegroundColor(const QVariant &category) const;
  QColor BackgroundColor(const QVariant &category) const;
  Style TextStyle(const QVariant &category) const;
};

CodeViewTheme GetCodeViewTheme(const bool &dark);

}  // namespace mx::gui
