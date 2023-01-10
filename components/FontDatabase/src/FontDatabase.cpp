/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/ui/FontDatabase.h>
#include <multiplier/ui/Assert.h>

#include <QFontDatabase>

namespace mx::gui {

namespace {

const std::vector<QString> kFontPathList{
    ":/Fonts/Source_Code_Pro/static/SourceCodePro-Black.ttf",
    ":/Fonts/Source_Code_Pro/static/SourceCodePro-BlackItalic.ttf",
    ":/Fonts/Source_Code_Pro/static/SourceCodePro-Bold.ttf",
    ":/Fonts/Source_Code_Pro/static/SourceCodePro-BoldItalic.ttf",
    ":/Fonts/Source_Code_Pro/static/SourceCodePro-ExtraBold.ttf",
    ":/Fonts/Source_Code_Pro/static/SourceCodePro-ExtraBoldItalic.ttf",
    ":/Fonts/Source_Code_Pro/static/SourceCodePro-ExtraLight.ttf",
    ":/Fonts/Source_Code_Pro/static/SourceCodePro-ExtraLightItalic.ttf",
    ":/Fonts/Source_Code_Pro/static/SourceCodePro-Italic.ttf",
    ":/Fonts/Source_Code_Pro/static/SourceCodePro-Light.ttf",
    ":/Fonts/Source_Code_Pro/static/SourceCodePro-LightItalic.ttf",
    ":/Fonts/Source_Code_Pro/static/SourceCodePro-Medium.ttf",
    ":/Fonts/Source_Code_Pro/static/SourceCodePro-MediumItalic.ttf",
    ":/Fonts/Source_Code_Pro/static/SourceCodePro-Regular.ttf",
    ":/Fonts/Source_Code_Pro/static/SourceCodePro-SemiBold.ttf",
    ":/Fonts/Source_Code_Pro/static/SourceCodePro-SemiBoldItalic.ttf",
};

}

void InitializeFontDatabase() {
  for (const auto &font_path : kFontPathList) {
    auto ret = QFontDatabase::addApplicationFont(font_path);
    Assert(ret != -1, "Failed to initialize the font database");
  }
}

}  // namespace mx::gui
