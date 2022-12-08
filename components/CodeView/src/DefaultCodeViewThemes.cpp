/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "DefaultCodeViewThemes.h"

namespace mx::gui {

// clang-format off
const CodeViewTheme kDefaultDarkCodeViewTheme{
  // default_background_color
  QColor::fromRgb(20, 20, 20),

  // default_foreground_color
  QColor::fromRgb(255, 255, 255),

  // token_style_map
  {
    { TokenCategory::kPreProcessorKeyword, { true, false, false, false } },
    { TokenCategory::kTypeAlias, { true, false, false, false } },
    { TokenCategory::kClass, { true, false, false, false } },
    { TokenCategory::kStruct, { true, false, false, false } },
    { TokenCategory::kUnion, { true, false, false, false } },
    { TokenCategory::kInterface, { true, false, false, false } },
    { TokenCategory::kFunction, { true, false, false, false } },
    { TokenCategory::kInstanceMethod, { true, false, false, false } },
    { TokenCategory::kGlobalVariable, { true, false, false, true } },
    { TokenCategory::kClassMethod, { true, false, false, true } },
    { TokenCategory::kEnumerator, { false, false, false, true } },
    { TokenCategory::kClassMember, { false, false, false, true } },
    { TokenCategory::kTemplateParameterValue, { false, false, false, true } },
  },

  // token_background_color_map
  { },

  {
    { TokenCategory::kUnknown, QColor::fromRgb(20, 20, 20) },
    { TokenCategory::kIdentifier, QColor::fromRgb(114, 114, 114) },
    { TokenCategory::kMacroName, QColor::fromRgb(121, 244, 241) },
    { TokenCategory::kKeyword, QColor::fromRgb(181, 116, 122) },
    { TokenCategory::kObjectiveCKeyword, QColor::fromRgb(181, 116, 122) },
    { TokenCategory::kPreProcessorKeyword, QColor::fromRgb(114, 111, 58) },
    { TokenCategory::kBuiltinTypeName, QColor::fromRgb(115, 61, 60) },
    { TokenCategory::kPunctuation, QColor::fromRgb(93, 93, 93) },
    { TokenCategory::kLiteral, QColor::fromRgb(226, 211, 148) },
    { TokenCategory::kComment, QColor::fromRgb(105, 104, 97) },
    { TokenCategory::kLocalVariable, QColor::fromRgb(198, 125, 237) },
    { TokenCategory::kGlobalVariable, QColor::fromRgb(198, 163, 73) },
    { TokenCategory::kParameterVariable, QColor::fromRgb(172, 122, 180) },
    { TokenCategory::kInstanceMethod, QColor::fromRgb(126, 125, 186) },
    { TokenCategory::kFunction, QColor::fromRgb(126, 125, 186) },
    { TokenCategory::kInstanceMember, QColor::fromRgb(207, 130, 235) },
    { TokenCategory::kClassMethod, QColor::fromRgb(170, 129, 52) },
    { TokenCategory::kClassMember, QColor::fromRgb(170, 129, 52) },
    { TokenCategory::kThis, QColor::fromRgb(181, 116, 122) },
    { TokenCategory::kClass, QColor::fromRgb(0, 177, 110) },
    { TokenCategory::kStruct, QColor::fromRgb(0, 177, 110) },
    { TokenCategory::kUnion, QColor::fromRgb(0, 177, 110) },
    { TokenCategory::kInterface, QColor::fromRgb(0, 177, 110) },
    { TokenCategory::kEnum, QColor::fromRgb(175, 144, 65) },
    { TokenCategory::kEnumerator, QColor::fromRgb(113, 163, 98) },
    { TokenCategory::kNamespace, QColor::fromRgb(95, 154, 160) },
    { TokenCategory::kTypeAlias, QColor::fromRgb(3, 171, 108) },
    { TokenCategory::kTemplateParameterType, QColor::fromRgb(198, 117, 29) },
    { TokenCategory::kTemplateParameterValue, QColor::fromRgb(174, 144, 65) },
    { TokenCategory::kLabel, QColor::fromRgb(149, 149, 149) },
  }
};
// clang-format on

// TODO(alessandro): Add a proper light theme
const CodeViewTheme kDefaultLightCodeViewTheme{kDefaultDarkCodeViewTheme};

}  // namespace mx::gui
