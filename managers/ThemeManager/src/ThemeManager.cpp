/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "FontSizeProxy.h"
#include "ProxyTheme.h"

#include <QAction>
#include <QActionGroup>
#include <QKeySequence>
#include <QMenu>

namespace mx::gui {

class ThemeManagerImpl final {
 public:
  ThemeManagerImpl(QApplication &application_)
      : application(application_) {}

  QApplication &application;

  std::vector<std::unique_ptr<ITheme>> themes;
  std::unique_ptr<ProxyTheme> proxy_theme;

  ITheme *current_theme{nullptr};

  // Lazily created when the user first changes font size.
  FontSizeProxy *font_size_proxy{nullptr};
};

ThemeManager::~ThemeManager(void) {}

ThemeManager::ThemeManager(QApplication &application, QObject *parent)
    : QObject(parent),
      d(std::make_shared<ThemeManagerImpl>(application)) {

  d->proxy_theme.reset(new ProxyTheme(nullptr));

  connect(d->proxy_theme.get(), &ProxyTheme::UninstallProxy,
          this, [this] (void) {
                  d->current_theme = d->proxy_theme->current_theme;
                  d->current_theme->Apply(d->application);
                  emit ThemeChanged(*this);
                });

  connect(d->proxy_theme.get(), &ITheme::ThemeChanged,
          this, [this] (void) {
                  d->proxy_theme->current_theme->Apply(d->application);
                  emit ThemeChanged(*this);
                });
}

//! Register a theme with the manager.
void ThemeManager::Register(std::unique_ptr<ITheme> theme) {

  auto raw_theme_ptr = d->themes.emplace_back(std::move(theme)).get();

  // NOTE(pag): We take ownership of memory management of themes. Don't let
  //            Qt's `QObjectPrivate::deleteChildren` be responsible for
  //            deleting these.
  raw_theme_ptr->setParent(nullptr);

  // Connect the internal theme change of the theme itself to a publication
  // of a full theme change to the rest of the app.
  connect(raw_theme_ptr, &ITheme::ThemeChanged,
          this, [raw_theme_ptr, this] (void) {
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

// Lazily create and install the font size proxy.
static FontSizeProxy *EnsureFontSizeProxy(ThemeManager *self,
                                          ThemeManagerImpl *d) {
  if (!d->font_size_proxy) {
    auto proxy = std::make_unique<FontSizeProxy>();
    d->font_size_proxy = proxy.get();
    self->AddProxy(std::move(proxy));
  }
  return d->font_size_proxy;
}

void ThemeManager::PopulateViewMenu(QMenu *menu) {
  // --- Theme selection submenu ---
  auto theme_menu = new QMenu(tr("Themes"), menu);
  menu->addMenu(theme_menu);

  auto populate_theme_menu = [this, theme_menu] (const ThemeManager &) {
    theme_menu->clear();
    auto theme_group = new QActionGroup(theme_menu);
    theme_group->setExclusive(true);

    auto current = Theme();
    for (auto &theme : ThemeList()) {
      auto action = new QAction(theme->Name(), theme_group);
      action->setCheckable(true);
      action->setChecked(theme.get() == current.get());

      connect(action, &QAction::triggered, this,
              [this, theme = std::move(theme)] () {
                SetTheme(theme);
              });

      theme_menu->addAction(action);
    }
  };

  populate_theme_menu(*this);
  connect(this, &ThemeManager::ThemeListChanged, this, populate_theme_menu);
  connect(this, &ThemeManager::ThemeChanged, this, populate_theme_menu);

  // --- Font size controls ---
  menu->addSeparator();

  auto increase_font = new QAction(tr("Increase Font Size"), menu);
  increase_font->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Plus));
  menu->addAction(increase_font);
  connect(increase_font, &QAction::triggered, this, [this]() {
    EnsureFontSizeProxy(this, d.get())->Increment();
  });

  auto decrease_font = new QAction(tr("Decrease Font Size"), menu);
  decrease_font->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus));
  menu->addAction(decrease_font);
  connect(decrease_font, &QAction::triggered, this, [this]() {
    EnsureFontSizeProxy(this, d.get())->Decrement();
  });

  auto reset_font = new QAction(tr("Reset Font Size"), menu);
  reset_font->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
  menu->addAction(reset_font);
  connect(reset_font, &QAction::triggered, this, [this]() {
    EnsureFontSizeProxy(this, d.get())->Reset();
  });
}

int ThemeManager::FontSizeDelta(void) const {
  if (d->font_size_proxy) {
    return d->font_size_proxy->Delta();
  }
  return 0;
}

void ThemeManager::SetFontSizeDelta(int delta) {
  if (delta != 0) {
    EnsureFontSizeProxy(this, d.get())->SetDelta(delta);
  } else if (d->font_size_proxy) {
    d->font_size_proxy->Reset();
  }
}

}  // namespace mx::gui
