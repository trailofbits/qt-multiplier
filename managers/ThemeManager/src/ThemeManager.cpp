/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "ProxyTheme.h"

namespace mx::gui {

class ThemeManagerImpl final {
 public:
  ThemeManagerImpl(QApplication &application_)
      : application(application_) {}

  QApplication &application;

  std::vector<std::unique_ptr<ITheme>> themes;
  std::unique_ptr<ProxyTheme> proxy_theme;

  ITheme *current_theme{nullptr};
};

ThemeManager::~ThemeManager(void) {}

ThemeManager::ThemeManager(QApplication &application, QObject *parent)
    : QObject(parent),
      d(std::make_shared<ThemeManagerImpl>(application)) {

  d->proxy_theme.reset(new ProxyTheme(nullptr, this));

  connect(d->proxy_theme.get(), &ProxyTheme::UninstallProxy,
          [this] (void) {
            d->current_theme = d->proxy_theme->current_theme;
            d->current_theme->Apply(d->application);
            emit ThemeChanged(*this);
          });

  connect(d->proxy_theme.get(), &ITheme::ThemeChanged,
          [this] (void) {
            d->proxy_theme->current_theme->Apply(d->application);
            emit ThemeChanged(*this);
          });
}

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
              emit ThemeChanged(*this);
            }
          });

  emit ThemeListChanged(*this);

  if (!d->current_theme) {
    d->current_theme = const_cast<ITheme *>(raw_theme_ptr);
    d->proxy_theme->current_theme = d->current_theme;
    d->current_theme->Apply(d->application);
    emit ThemeChanged(*this);
  }
}

//! Add a theme proxy to the manager. This wraps whatever theme or theme
//! proxies are already present. Ownership of the proxy is given to the
//! theme manager, which shares ownership back with the creator of the proxy.
void ThemeManager::AddProxy(IThemeProxyPtr proxy) {
  d->proxy_theme->Add(std::move(proxy));

  if (!dynamic_cast<ProxyTheme *>(d->current_theme)) {
    d->proxy_theme->current_theme = d->current_theme;
    d->current_theme = d->proxy_theme.get();
  }

  d->current_theme->Apply(d->application);
  emit ThemeChanged(*this);
}

//! Sets the active theme. This is a no-op if `theme` is not owned by this
//! theme manager.
void ThemeManager::SetTheme(IThemePtr theme) {
  auto raw_theme_ptr = const_cast<ITheme *>(theme.get());
  if (!theme || raw_theme_ptr == d->current_theme ||
      theme.get() == d->proxy_theme.get()) {
    return;
  }

  for (const auto &owned_theme : d->themes) {
    if (owned_theme.get() != raw_theme_ptr) {
      continue;
    }

    d->proxy_theme->current_theme = raw_theme_ptr;
    if (!dynamic_cast<ProxyTheme *>(d->current_theme)) {
      d->current_theme = raw_theme_ptr;
    }
    
    d->current_theme->Apply(d->application);
    emit ThemeChanged(*this);
    break;
  }
}

//! Returns the active CodeViewTheme
IThemePtr ThemeManager::Theme(void) const {
  if (!d->current_theme) {
    return {};
  }
  return IThemePtr(d, d->current_theme);
}

//! Look up a theme by its id, e.g. `com.trailofbits.theme.Dark`. Returns
//! `nullptr` on failure.
IThemePtr ThemeManager::Find(const QString &id) const {
  for (const auto &owned_theme : d->themes) {
    if (owned_theme->Id() == id) {
      return IThemePtr(d, owned_theme.get());
    }
  }
  return {};
}

// Return the list of registered theme IDs.
std::vector<IThemePtr> ThemeManager::ThemeList(void) const {
  std::vector<IThemePtr> themes;
  for (const auto &owned_theme : d->themes) {
    themes.emplace_back(IThemePtr(d, owned_theme.get()));
  }
  return themes;
}

}  // namespace mx::gui
