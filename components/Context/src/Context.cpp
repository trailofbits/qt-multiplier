// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Context.h>

#include <multiplier/Frontend/File.h>
#include <multiplier/Index.h>

#include <multiplier/GUI/Managers/ActionManager.h>
#include <multiplier/GUI/ThemeManager.h>

namespace mx::gui {

struct Context::PrivateData {
  class ActionManager action_registry;
  class Index index;
  class FileLocationCache file_location_cache;
  class ThemeManager &theme_manager;

  inline PrivateData(const class Index &index_)
      : index(index_),
        theme_manager(ThemeManager::Get()) {}
};

Context::~Context(void) {}

Context::Context(const class Index &index_)
    : d(new PrivateData(index_)) {}

class ActionManager &Context::ActionManager(void) const noexcept {
  return d->action_registry;
}

TriggerHandle Context::RegisterAction(IAction &action) const noexcept {
  return d->action_registry.Register(action);
}

TriggerHandle Context::FindAction(const QString &action) const noexcept {
  return d->action_registry.Find(action);
}

// Return the current index being used.
const class Index &Context::Index(void) const noexcept {
  return d->index;
}

// Return the shared file location cache.
const class FileLocationCache &Context::FileLocationCache(void) const noexcept {
  return d->file_location_cache;
}

// Get access to the global theme manager.
ThemeManager &Context::ThemeManager(void) const noexcept {
  return d->theme_manager;
}

}  // namespace mx::gui
