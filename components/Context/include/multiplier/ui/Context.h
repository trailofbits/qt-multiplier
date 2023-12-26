// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <memory>
#include <QString>

namespace mx {
class FileLocationCache;
class Index;
}  // namespace mx
namespace mx::gui {

class ActionRegistry;
class IAction;
class IThemeManager;

// Global UI context. There is one context per application.
class Context {
  struct PrivateData;
  const std::unique_ptr<PrivateData> d;

  Context(const Context &) = delete;
  Context(Context &&) noexcept = delete;

 public:
  ~Context(void);
  
  explicit Context(const class Index &index_);

  class ActionRegistry &ActionRegistry(void) const noexcept;

  IAction &Action(const QString &action) const noexcept;

  // Return the current index being used.
  const class Index &Index(void) const noexcept;

  // Return the shared file location cache.
  const class FileLocationCache &FileLocationCache(void) const noexcept;

  // Get access to the global theme manager.
  IThemeManager &ThemeManager(void) const noexcept;
};

}  // namespace mx::gui
