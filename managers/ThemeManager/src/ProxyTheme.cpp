/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "ProxyTheme.h"

namespace mx::gui {

ProxyTheme::~ProxyTheme(void) {}

ProxyTheme::ProxyTheme(ITheme *current_theme_, QObject *parent)
    : ITheme(parent),
      current_theme(current_theme_) {}

void ProxyTheme::Add(IThemeProxyPtr proxy) {
  auto raw_proxy_ptr = proxies.emplace_back(std::move(proxy)).get();

  // NOTE(pag): We take ownership of memory management of themes. Don't let
  //            Qt's `QObjectPrivate::deleteChildren` be responsible for
  //            deleting these.
  raw_proxy_ptr->setParent(nullptr);

  // Forward a proxy change into a theme change.
  connect(raw_proxy_ptr, &IThemeProxy::ThemeProxyChanged,
          this, &ITheme::ThemeChanged);

  connect(raw_proxy_ptr, &IThemeProxy::Uninstall,
          this, &ProxyTheme::Remove);
}

void ProxyTheme::Remove(IThemeProxy *proxy) {
  std::vector<IThemeProxyPtr> new_proxies;
  for (auto &old_proxy : proxies) {
    if (old_proxy.get() != proxy) {
      new_proxies.emplace_back(std::move(old_proxy));
    }
  }

  proxies.swap(new_proxies);
  if (proxies.empty()) {
    emit UninstallProxy();
  } else {
    emit ThemeChanged();
  }
}

void ProxyTheme::Apply(QApplication &application) {
  current_theme->Apply(application);
}

const QPalette &ProxyTheme::Palette(void) const {
  return current_theme->Palette();
}

QString ProxyTheme::Name(void) const {
  return current_theme->Name();
}

QString ProxyTheme::Id(void) const {
  return current_theme->Id();
}

QFont ProxyTheme::Font(void) const {
  auto font = current_theme->Font();
  for (auto it = proxies.rbegin(); it != proxies.rend(); ++it) {
    font = (*it)->Font(std::move(font));
  }
  return font;
}

QColor ProxyTheme::CursorColor(void) const {
  auto color = current_theme->CursorColor();
  for (auto it = proxies.rbegin(); it != proxies.rend(); ++it) {
    color = (*it)->CursorColor(std::move(color));
  }
  return color;
}

QColor ProxyTheme::SelectionColor(void) const {
  auto color = current_theme->SelectionColor();
  for (auto it = proxies.rbegin(); it != proxies.rend(); ++it) {
    color = (*it)->SelectionColor(std::move(color));
  }
  return color;
}

QColor ProxyTheme::IconColor(IconStyle style) const {
  auto color = current_theme->IconColor(style);
  for (auto it = proxies.rbegin(); it != proxies.rend(); ++it) {
    color = (*it)->IconColor(std::move(color), style);
  }
  return color;
}

QColor ProxyTheme::GutterForegroundColor(void) const {
  auto color = current_theme->GutterForegroundColor();
  for (auto it = proxies.rbegin(); it != proxies.rend(); ++it) {
    color = (*it)->GutterForegroundColor(std::move(color));
  }
  return color;
}

QColor ProxyTheme::GutterBackgroundColor(void) const {
  auto color = current_theme->GutterBackgroundColor();
  for (auto it = proxies.rbegin(); it != proxies.rend(); ++it) {
    color = (*it)->GutterBackgroundColor(std::move(color));
  }
  return color;
}

QColor ProxyTheme::DefaultForegroundColor(void) const {
  auto color = current_theme->DefaultForegroundColor();
  for (auto it = proxies.rbegin(); it != proxies.rend(); ++it) {
    color = (*it)->DefaultForegroundColor(std::move(color));
  }
  return color;
}

QColor ProxyTheme::DefaultBackgroundColor(void) const {
  auto color = current_theme->DefaultBackgroundColor();
  for (auto it = proxies.rbegin(); it != proxies.rend(); ++it) {
    color = (*it)->DefaultBackgroundColor(std::move(color));
  }
  return color;
}

QColor ProxyTheme::CurrentLineBackgroundColor(void) const {
  auto color = current_theme->CurrentLineBackgroundColor();
  for (auto it = proxies.rbegin(); it != proxies.rend(); ++it) {
    color = (*it)->CurrentLineBackgroundColor(std::move(color));
  }
  return color;
}

QColor ProxyTheme::CurrentEntityBackgroundColor(
    const VariantEntity &entity) const {
  auto color = current_theme->CurrentEntityBackgroundColor(entity);
  for (auto it = proxies.rbegin(); it != proxies.rend(); ++it) {
    color = (*it)->CurrentEntityBackgroundColor(std::move(color), entity);
  }
  return color;
}

ITheme::ColorAndStyle ProxyTheme::TokenColorAndStyle(const Token &token) const {
  auto cs = current_theme->TokenColorAndStyle(token);
  for (auto it = proxies.rbegin(); it != proxies.rend(); ++it) {
    cs = (*it)->TokenColorAndStyle(std::move(cs), token);
  }
  return cs;
}

std::optional<QColor> ProxyTheme::EntityBackgroundColor(
    const VariantEntity &entity) const {
  auto color = current_theme->EntityBackgroundColor(entity);
  for (auto it = proxies.rbegin(); it != proxies.rend(); ++it) {
    color = (*it)->EntityBackgroundColor(std::move(color), entity);
  }
  return color;
}

}  // namespace mx::gui
