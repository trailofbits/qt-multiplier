/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "ThemeManager.h"

#include <multiplier/ui/IThemeManager.h>
#include <multiplier/ui/Assert.h>

namespace mx::gui {

namespace {

QApplication *application_ptr{nullptr};

}

void IThemeManager::Initialize(QApplication &application) {
  Assert(application_ptr == nullptr,
         "The theme manager was not initialized twice");

  application_ptr = &application;
}

IThemeManager &IThemeManager::Get() {
  Assert(
      application_ptr != nullptr,
      "The theme manager was not initialized with IThemeManager::Initialize");

  static ThemeManager theme_manager(*application_ptr);
  return theme_manager;
}

}  // namespace mx::gui
