/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/GUI/Managers/ThemeManager.h>

namespace mx::gui {

class MediaManager;

std::unique_ptr<ITheme> CreateDarkTheme(const MediaManager &);
std::unique_ptr<ITheme> CreateLightTheme(const MediaManager &);

}  // namespace mx::gui
