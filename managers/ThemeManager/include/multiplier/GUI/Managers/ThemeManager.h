/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QAbstractItemView>

#include <memory>
#include <multiplier/GUI/Interfaces/IThemeProxy.h>
#include <vector>

namespace mx::gui {

class ThemeManagerImpl;

//! Manages the themes.
class ThemeManager Q_DECL_FINAL : public QObject {
  Q_OBJECT

  const std::shared_ptr<ThemeManagerImpl> d;

 public:
  virtual ~ThemeManager(void);

  explicit ThemeManager(QApplication &application, QObject *parent = nullptr);

  //! Register a theme with the manager. The first registered theme is set to
  //! be the current theme.
  void Register(std::unique_ptr<ITheme> theme);

  //! Sets the active theme. This is a no-op if `theme` is not owned by this
  //! theme manager.
  void SetTheme(IThemePtr theme);

  //! Add a theme proxy to the manager. This wraps whatever theme or theme
  //! proxies are already present.
  void AddProxy(IThemeProxyPtr proxy);

  //! Returns the active theme.
  IThemePtr Theme(void) const;

  //! Look up a theme by its ID, e.g. `com.trailofbits.theme.Dark`.
  IThemePtr Find(const QString &id) const;

  // Return the list of registered theme IDs.
  std::vector<IThemePtr> ThemeList(void) const;

 signals:
  //! Emitted when the theme has changed. Sends out the new theme.
  void ThemeChanged(const ThemeManager &theme_manager);

  //! Emitted when the set of themes change. Anyone who is listening to this
  //! signal should hold a reference to the `ThemeManager`, so that upon
  //! receipt of the signal, they can ask for the new pallette and code view
  //! theme.
  void ThemeListChanged(const ThemeManager &theme_manager);
};

}  // namespace mx::gui
