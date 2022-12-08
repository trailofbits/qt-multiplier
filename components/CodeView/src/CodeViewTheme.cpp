/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/ui/CodeViewTheme.h>

#include "DefaultCodeViewThemes.h"

namespace mx::gui {

CodeViewTheme GetDefaultTheme(bool dark_mode) {
  if (dark_mode) {
    return kDefaultDarkCodeViewTheme;
  } else {
    return kDefaultLightCodeViewTheme;
  }
}

}  // namespace mx::gui
