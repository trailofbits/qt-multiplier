/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "ThemeManager.h"
#include "Theme.h"

#ifdef __APPLE__
#  include "macos_utils.h"
#endif

namespace mx::gui {

struct ThemeManager::PrivateData final {
  PrivateData(QApplication &application_) : application(application_) {}

  QApplication &application;

  bool is_dark_theme{false};
  QPalette palette;
  CodeViewTheme code_view_theme;
};

void ThemeManager::SetTheme(const bool &dark) {
  d->is_dark_theme = dark;

#ifdef __APPLE__
  auto ns_app_theme = d->is_dark_theme ? NSAppTheme::Dark : NSAppTheme::Light;
  mx::gui::SetNSAppTheme(ns_app_theme);
#endif

  d->palette = d->is_dark_theme ? GetDarkPalette() : GetLightPalette();
  d->code_view_theme = mx::gui::GetCodeViewTheme(d->is_dark_theme);
  d->application.setPalette(d->palette);

  emit ThemeChanged(d->palette, d->code_view_theme);
}

const QPalette &ThemeManager::GetPalette() const {
  return d->palette;
}

const CodeViewTheme &ThemeManager::GetCodeViewTheme() const {
  return d->code_view_theme;
}

bool ThemeManager::isDarkTheme() const {
  return d->is_dark_theme;
}

ThemeManager::~ThemeManager() {}

ThemeManager::ThemeManager(QApplication &application)
    : d(new PrivateData(application)) {}

}  // namespace mx::gui
