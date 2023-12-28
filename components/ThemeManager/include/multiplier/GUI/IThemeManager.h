/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/CodeViewTheme.h>

#include <QObject>
#include <QPalette>
#include <QApplication>

namespace mx::gui {

//! The reference explorer widget
class IThemeManager : public QObject {
  Q_OBJECT

 public:
  //! Initialization method
  static void Initialize(QApplication &application);

  //! Returns an instance of the IThemeManager
  static IThemeManager &Get();

  //! Sets the active theme
  virtual void SetTheme(const bool &dark) = 0;

  //! Returns the active QPalette
  virtual const QPalette &GetPalette() const = 0;

  //! Returns the active CodeViewTheme
  virtual const CodeViewTheme &GetCodeViewTheme() const = 0;

  //! Sends a ThemeChanged update to all connected components
  virtual void SendGlobalUpdate() const = 0;

  //! Returns true if the active theme is dark
  virtual bool isDarkTheme() const = 0;

  //! Constructor
  IThemeManager() = default;

  //! Destructor
  virtual ~IThemeManager() override = default;

  //! Disabled copy constructor
  IThemeManager(const IThemeManager &) = delete;

  //! Disabled copy assignment operator
  IThemeManager &operator=(const IThemeManager &) = delete;

 signals:
  //! Emitted when the selected item has changed
  void ThemeChanged(const QPalette &palette,
                    const CodeViewTheme &code_view_theme) const;
};

}  // namespace mx::gui
