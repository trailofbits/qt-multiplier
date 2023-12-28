// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "macos_utils.h"

#include <AppKit/AppKit.h>

namespace mx::gui {

void SetNSAppTheme(const NSAppTheme &theme) {
  auto theme_name = 
    (theme == NSAppTheme::Dark) ? NSAppearanceNameDarkAqua :
                                  NSAppearanceNameAqua;

  NSApp.appearance = [NSAppearance appearanceNamed:theme_name];
}

}  // namespace mx::gui
