// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Managers/ConfigManager.h>

#include <multiplier/Frontend/File.h>
#include <multiplier/Index.h>

#include <multiplier/GUI/Managers/ActionManager.h>
#include <multiplier/GUI/Managers/MediaManager.h>
#include <multiplier/GUI/Managers/ThemeManager.h>

#include "ThemedItemDelegate.h"

namespace mx::gui {

class ConfigManagerImpl {
 public:
  class ThemeManager theme_manager;
  class MediaManager media_manager;
  class ActionManager action_manager;
  class FileLocationCache file_location_cache;
  class Index index;

  inline ConfigManagerImpl(QApplication &application, QObject *self)
      : theme_manager(application, self),
        media_manager(theme_manager, self) {}
};

ConfigManager::~ConfigManager(void) {}

ConfigManager::ConfigManager(QApplication &application, QObject *parent)
    : QObject(parent),
      d(std::make_shared<ConfigManagerImpl>(application, this)) {}

class ActionManager &ConfigManager::ActionManager(void) const noexcept {
  return d->action_manager;
}

// Get access to the global theme manager.
class ThemeManager &ConfigManager::ThemeManager(void) const noexcept {
  return d->theme_manager;
}

// Get access to the global media manager.
class MediaManager &ConfigManager::MediaManager(void) const noexcept {
  return d->media_manager;
}

//! Get access to the current index.
const class Index &ConfigManager::Index(void) const noexcept {
  return d->index;
}

//! Change the current index.
void ConfigManager::SetIndex(const class Index &index) noexcept {
  d->file_location_cache.clear();
  d->index = index;
  emit IndexChanged(*this);
}

// Return the shared file location cache.
const class FileLocationCache &
ConfigManager::FileLocationCache(void) const noexcept {
  return d->file_location_cache;
}

//! Set an item delegate on `view` that pays attention to the theme. This
//! allows items using `IModel` to present tokens.
//!
//! NOTE(pag): This should only be applied to views backed by `IModel`s,
//!            either directly or by proxy.
void ConfigManager::InstallItemDelegate(
    QAbstractItemView *view, const ItemDelegateConfig &config) const {

  auto set_delegate = [=] (const class ThemeManager &self) {
    QAbstractItemDelegate *old_delegate = view->itemDelegate();

    auto theme = self.Theme();
    view->setFont(theme->Font());

    auto new_delegate = new ThemedItemDelegate(
        std::move(theme), config.whitespace_replacement,
        4u  /* TODO(pag): Take from config manager. */, view);
    view->setItemDelegate(new_delegate);

    if (old_delegate) {
      old_delegate->deleteLater();
    }
  };

  set_delegate(d->theme_manager);
  connect(&(d->theme_manager), &ThemeManager::ThemeChanged,
          view, std::move(set_delegate));
}

}  // namespace mx::gui
