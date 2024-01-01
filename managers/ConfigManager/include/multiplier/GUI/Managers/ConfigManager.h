// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <memory>
#include <optional>

#include <QAbstractItemView>
#include <QApplication>
#include <QObject>
#include <QString>

namespace mx {
class FileLocationCache;
class Index;
}  // namespace mx
namespace mx::gui {

class ActionManager;
class ConfigManagerImpl;
class MediaManager;
class ThemeManager;
class TriggerHandle;

// Manages the global configuration.
class ConfigManager Q_DECL_FINAL : public QObject {
  Q_OBJECT

  std::shared_ptr<ConfigManagerImpl> d;

 public:
  virtual ~ConfigManager(void);
  
  explicit ConfigManager(QApplication &application, QObject *parent = nullptr);

  //! Get access to the global action manager.
  class ActionManager &ActionManager(void) const noexcept;

  //! Get access to the global theme manager.
  class ThemeManager &ThemeManager(void) const noexcept;

  //! Get access to the global media manager.
  class MediaManager &MediaManager(void) const noexcept;

  //! Get access to the current index.
  const class Index &Index(void) const noexcept;

  //! Change the current index.
  void SetIndex(const class Index &index) noexcept;

  //! Return the shared file location cache. This is used to compute locations
  //! of things, taking into account the current configuration (tab width, and
  //! tab stops).
  const class FileLocationCache &FileLocationCache(void) const noexcept;

  //! Configuration for item delegates.
  struct ItemDelegateConfig {

    //! If present, then whitespace is replaced by this.
    std::optional<std::string> whitespace_replacement;
  };

  //! Set an item delegate on `view` that pays attention to the theme. This
  //! allows items using `IModel` to present tokens.
  //!
  //! NOTE(pag): This will try to proxy any pre-existing item delegates.
  //!
  //! NOTE(pag): This should only be applied to views backed by `IModel`s,
  //!            either directly or by proxy.
  void InstallItemDelegate(QAbstractItemView *view,
                           const ItemDelegateConfig &config={}) const;

 signals:
  void IndexChanged(const ConfigManager &config_manager);
};

}  // namespace mx::gui
