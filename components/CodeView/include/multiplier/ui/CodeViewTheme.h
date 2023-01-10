/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/Util.h>

#include <filesystem>
#include <optional>
#include <unordered_map>

#include <QColor>

namespace mx::gui {

struct CodeViewTheme final {
  struct Style final {
    bool bold{false};
    bool underline{false};
    bool strikeout{false};
    bool italic{false};
  };

  QColor default_background_color;
  QColor default_foreground_color;

  std::unordered_map<TokenCategory, Style> token_style_map;
  std::unordered_map<TokenCategory, QColor> token_background_color_map;
  std::unordered_map<TokenCategory, QColor> token_foreground_color_map;
};

// TODO(alessandro): Implement function that loads a theme from file
CodeViewTheme GetDefaultTheme(bool dark_mode);

}  // namespace mx::gui
