/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/GUI/Managers/ThemeManager.h>

namespace mx::gui {

struct ThemeManager::PrivateData final
    : std::enable_shared_from_this<ThemeManager::PrivateData> {

  PrivateData(QApplication &application_)
      : application(application_) {}

  QApplication &application;

  std::vector<std::unique_ptr<ITheme>> themes;

  const ITheme *current_theme{nullptr};
};

ThemeManager::~ThemeManager(void) {}

ThemeManager::ThemeManager(QApplication &application)
    : QObject(&application),
      d((new PrivateData(application))->shared_from_this()) {}

//! Register a theme with the manager.
void ThemeManager::Register(std::unique_ptr<ITheme> theme) {

  auto raw_theme_ptr = d->themes.emplace_back(std::move(theme)).get();
  raw_theme_ptr->setParent(this);

  // Connect the internal theme change of the theme itself to a publication
  // of a full theme change to the rest of the app.
  connect(raw_theme_ptr, &ITheme::ThemeChanged,
          [raw_theme_ptr, this] (void) {
            if (d->current_theme == raw_theme_ptr) {
              raw_theme_ptr->Apply(d->application);
              emit ThemeChanged(ITheme::Ptr(d, raw_theme_ptr));
            }
          });

  emit ThemeListChanged(ThemeList());

  if (!d->current_theme) {
    d->current_theme = raw_theme_ptr;
    raw_theme_ptr->Apply(d->application);
    emit ThemeChanged(ITheme::Ptr(d, raw_theme_ptr));
  }
}

//! Sets the active theme. This is a no-op if `theme` is not owned by this
//! theme manager.
void ThemeManager::SetTheme(ITheme::Ptr theme) {
  if (!theme || theme.get() == d->current_theme) {
    return;
  }

  for (const auto &owned_theme : d->themes) {
    if (owned_theme.get() == theme.get()) {
      d->current_theme = theme.get();
      const_cast<ITheme *>(d->current_theme)->Apply(d->application);
      emit ThemeChanged(ITheme::Ptr(d, d->current_theme));
      return;
    }
  }
}

//! Returns the active CodeViewTheme
ITheme::Ptr ThemeManager::Theme(void) const {
  if (!d->current_theme) {
    return {};
  }
  return ITheme::Ptr(d, d->current_theme);
}

//! Look up a theme by its id, e.g. `com.trailofbits.theme.Dark`. Returns
//! `nullptr` on failure.
ITheme::Ptr ThemeManager::Find(const QString &id) const {
  for (const auto &owned_theme : d->themes) {
    if (owned_theme->Id() == id) {
      return ITheme::Ptr(d, owned_theme.get());
    }
  }
  return {};
}

// Return the list of registered theme IDs.
std::vector<ITheme::Ptr> ThemeManager::ThemeList(void) const {
  std::vector<ITheme::Ptr> themes;
  for (const auto &owned_theme : d->themes) {
    themes.emplace_back(ITheme::Ptr(d, owned_theme.get()));
  }
  return themes;
}

}  // namespace mx::gui
