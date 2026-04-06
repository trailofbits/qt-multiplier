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
#include <QColor>
#include <QObject>
#include <QSet>
#include <QString>

#include <unordered_map>

#include <multiplier/Types.h>

class QMenu;

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

  //! Change the current index. `db_path` is used to derive the project
  //! settings file path (e.g., /path/to/foo.db -> /path/to/foo.qmx).
  void SetIndex(const class Index &index,
                const QString &db_path = {}) noexcept;

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
  //! NOTE(pag): This should only be applied to views backed by `IModel`s,
  //!            either directly or by proxy.
  //!
  //! NOTE(pag): This exists as a function of the `ConfigManager` and not the
  //!            `ThemeManager` because tab width might be configurable in the
  //!            future.
  void InstallItemDelegate(QAbstractItemView *view,
                           const ItemDelegateConfig &config={}) const;

  //! Let each manager populate a View menu with its relevant actions.
  void PopulateViewMenu(QMenu *menu);

  //! Get the current tab width (in spaces).
  unsigned TabWidth(void) const noexcept;

  //! Set the tab width (in spaces). Persists to settings.
  void SetTabWidth(unsigned width);

  //! Get/set whether tab stops are used for location calculations.
  bool UseTabStops(void) const noexcept;
  void SetUseTabStops(bool use);

  //! Save all persistent settings to disk.
  void SaveSettings(void) const;

  //! Load persistent settings from disk.
  void LoadSettings(void);

  //! Save/load header state (column ordering, widths, etc.) for a named view.
  void SaveHeaderState(const QString &id, const QByteArray &state) const;
  QByteArray LoadHeaderState(const QString &id) const;

  //! Save/load the main window layout (dock positions, sizes, visibility).
  void SaveWindowLayout(const QByteArray &state,
                        const QByteArray &geometry) const;
  bool LoadWindowLayout(QByteArray &state, QByteArray &geometry) const;

  //! Save/load per-project settings (sibling .qmx file next to the .db).
  void SaveProjectSettings(void) const;
  void LoadProjectSettings(void);

  //! Save/load expanded macros (per-project).
  void SaveExpandedMacros(const QSet<mx::RawEntityId> &macros) const;
  QSet<mx::RawEntityId> LoadExpandedMacros(void) const;

  //! Save/load entity highlight colors (per-project).
  using HighlightColorMap =
      std::unordered_map<mx::RawEntityId, std::pair<QColor, QColor>>;
  void SaveHighlightColors(const HighlightColorMap &colors) const;
  HighlightColorMap LoadHighlightColors(void) const;

  //! Save/load navigation history (per-project).
  struct NavigationEntry {
    mx::RawEntityId entity_id{mx::kInvalidEntityId};
    unsigned line{0};
    unsigned column{0};
    QString label;
  };
  void SaveNavigationHistory(
      const std::vector<NavigationEntry> &entries,
      const QString &key = QStringLiteral("Main")) const;
  std::vector<NavigationEntry> LoadNavigationHistory(
      const QString &key = QStringLiteral("Main")) const;

 signals:
  void IndexChanged(const ConfigManager &config_manager);
  void TabWidthChanged(unsigned tab_width);
  void UseTabStopsChanged(bool use_tab_stops);
};

}  // namespace mx::gui
