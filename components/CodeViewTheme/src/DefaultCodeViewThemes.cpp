/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "DefaultCodeViewThemes.h"

#include <multiplier/Entities/TokenCategory.h>

namespace mx::gui {

// clang-format off
const CodeViewTheme kDefaultDarkCodeViewTheme{
  // Font name
  "Source Code Pro",

  // selected_line_background_color
  QColor::fromRgb(5, 5, 5),

  // default_background_color
  QColor::fromRgb(20, 20, 20),

  // default_foreground_color
  QColor::fromRgb(255, 255, 255),

  // default_gutter_background
  QColor::fromRgb(20, 20, 20),

  // default_gutter_foreground
  QColor::fromRgb(128, 128, 128),

  // token_style_map
  {
    { TokenCategory::MACRO_DIRECTIVE_NAME, { true, false, false, false } },
    { TokenCategory::TYPE_ALIAS, { true, false, false, false } },
    { TokenCategory::CLASS, { true, false, false, false } },
    { TokenCategory::STRUCT, { true, false, false, false } },
    { TokenCategory::UNION, { true, false, false, false } },
    { TokenCategory::INTERFACE, { true, false, false, false } },
    { TokenCategory::CONCEPT, { true, false, false, false } },
    { TokenCategory::FUNCTION, { true, false, false, false } },
    { TokenCategory::INSTANCE_METHOD, { true, false, false, false } },
    { TokenCategory::GLOBAL_VARIABLE, { true, false, false, true } },
    { TokenCategory::CLASS_METHOD, { true, false, false, true } },
    { TokenCategory::CLASS_MEMBER, { false, false, false, true } },
    { TokenCategory::ENUMERATOR, { false, false, false, true } },
    { TokenCategory::TEMPLATE_PARAMETER_VALUE, { false, false, false, true } },
  },

  // token_background_color_map
  { },

  {
    { TokenCategory::UNKNOWN, QColor::fromRgb(20, 20, 20) },
    { TokenCategory::IDENTIFIER, QColor::fromRgb(114, 114, 114) },
    { TokenCategory::MACRO_NAME, QColor::fromRgb(121, 244, 241) },
    { TokenCategory::MACRO_PARAMETER_NAME, QColor::fromRgb(114, 111, 58) },
    { TokenCategory::MACRO_DIRECTIVE_NAME, QColor::fromRgb(114, 111, 58) },
    { TokenCategory::KEYWORD, QColor::fromRgb(181, 116, 122) },
    { TokenCategory::OBJECTIVE_C_KEYWORD, QColor::fromRgb(181, 116, 122) },
    { TokenCategory::BUILTIN_TYPE_NAME, QColor::fromRgb(115, 61, 60) },
    { TokenCategory::PUNCTUATION, QColor::fromRgb(93, 93, 93) },
    { TokenCategory::LITERAL, QColor::fromRgb(226, 211, 148) },
    { TokenCategory::COMMENT, QColor::fromRgb(105, 104, 97) },
    { TokenCategory::LOCAL_VARIABLE, QColor::fromRgb(198, 125, 237) },
    { TokenCategory::GLOBAL_VARIABLE, QColor::fromRgb(198, 163, 73) },
    { TokenCategory::PARAMETER_VARIABLE, QColor::fromRgb(172, 122, 180) },
    { TokenCategory::FUNCTION, QColor::fromRgb(126, 125, 186) },
    { TokenCategory::INSTANCE_METHOD, QColor::fromRgb(126, 125, 186) },
    { TokenCategory::INSTANCE_MEMBER, QColor::fromRgb(207, 130, 235) },
    { TokenCategory::CLASS_METHOD, QColor::fromRgb(170, 129, 52) },
    { TokenCategory::CLASS_MEMBER, QColor::fromRgb(170, 129, 52) },
    { TokenCategory::THIS, QColor::fromRgb(181, 116, 122) },
    { TokenCategory::CLASS, QColor::fromRgb(0, 177, 110) },
    { TokenCategory::STRUCT, QColor::fromRgb(0, 177, 110) },
    { TokenCategory::UNION, QColor::fromRgb(0, 177, 110) },
    { TokenCategory::INTERFACE, QColor::fromRgb(0, 177, 110) },
    { TokenCategory::CONCEPT, QColor::fromRgb(0, 177, 110) },
    { TokenCategory::ENUM, QColor::fromRgb(175, 144, 65) },
    { TokenCategory::ENUMERATOR, QColor::fromRgb(113, 163, 98) },
    { TokenCategory::NAMESPACE, QColor::fromRgb(95, 154, 160) },
    { TokenCategory::TYPE_ALIAS, QColor::fromRgb(3, 171, 108) },
    { TokenCategory::TEMPLATE_PARAMETER_TYPE, QColor::fromRgb(198, 117, 29) },
    { TokenCategory::TEMPLATE_PARAMETER_VALUE, QColor::fromRgb(174, 144, 65) },
    { TokenCategory::LABEL, QColor::fromRgb(149, 149, 149) },
  },

  // token_group_color_list
  {
      QColor::fromRgb(0x101119), QColor::fromRgb(0x0F171A),
      QColor::fromRgb(0x060E0B), QColor::fromRgb(0x1C1A0C),
      QColor::fromRgb(0x221607), QColor::fromRgb(0x291500),
      QColor::fromRgb(0x2B1412), QColor::fromRgb(0x2B121D),
      QColor::fromRgb(0x2C1125), QColor::fromRgb(0x1E0B1E),
  }

};
// clang-format on

// TODO(alessandro): Add a proper light theme
const CodeViewTheme kDefaultLightCodeViewTheme{kDefaultDarkCodeViewTheme};

}  // namespace mx::gui
