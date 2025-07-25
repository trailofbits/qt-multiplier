// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "MacosUtils.h"

#include <AppKit/AppKit.h>

namespace mx::gui {

bool IsNaturalScroll(void) {
  return [[[NSUserDefaults standardUserDefaults] objectForKey:@"com.apple.swipescrolldirection"] boolValue];
}

}  // namespace mx::gui
