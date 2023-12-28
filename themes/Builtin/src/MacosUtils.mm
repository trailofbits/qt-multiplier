// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "MacosUtils.h"

#include <AppKit/AppKit.h>

namespace mx::gui {

void SetNSAppToDarkTheme(void) {
  NSApp.appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
}

void SetNSAppToLightTheme(void) {
  NSApp.appearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua];
}

}  // namespace mx::gui
