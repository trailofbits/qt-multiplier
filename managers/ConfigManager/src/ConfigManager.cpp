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

struct ConfigManager::PrivateData {
  class ThemeManager theme_manager;
  class MediaManager media_manager;
  class ActionManager action_manager;
  class FileLocationCache file_location_cache;

  inline PrivateData(QApplication &application)
      : theme_manager(application),
        media_manager(theme_manager) {}
};

ConfigManager::~ConfigManager(void) {}

ConfigManager::ConfigManager(QApplication &application)
    : QObject(&application),
      d(new PrivateData(application)) {}

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

// Return the shared file location cache.
const class FileLocationCache &
ConfigManager::FileLocationCache(void) const noexcept {
  return d->file_location_cache;
}

//! Create an item delegate. Only one item delegate should be installed per
//! `QAbstractItemView` at a time.
void ConfigManager::InstallItemDelegate(
    QAbstractItemView *view, const ItemDelegateConfig &config) const {

  auto set_delegate = [=] (const class ThemeManager &self) {
    QAbstractItemDelegate *old_delegate = view->itemDelegate();
    auto old_themed_delegate = dynamic_cast<ThemedItemDelegate *>(old_delegate);

    // Try to proxy the old delegate that's there.
    QAbstractItemDelegate *prev_delegate = nullptr;
    if (old_themed_delegate) {
      prev_delegate = old_themed_delegate->prev_delegate;
      old_themed_delegate->prev_delegate = nullptr;

    } else if (old_delegate) {
      prev_delegate = old_delegate;
      old_delegate = nullptr;
    }

    auto new_delegate = new ThemedItemDelegate(
        self.Theme(), prev_delegate, config.whitespace_replacement,
        4u  /* TODO(pag): Take from config manager. */, view);
    view->setItemDelegate(new_delegate);

    if (old_delegate) {
      old_delegate->deleteLater();
    }
  };

  set_delegate(d->theme_manager);
  connect(&(d->theme_manager), &ThemeManager::ThemeChanged,
          std::move(set_delegate));
}

}  // namespace mx::gui
