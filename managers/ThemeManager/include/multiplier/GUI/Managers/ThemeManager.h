/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QApplication>
#include <QFont>
#include <QObject>
#include <QPalette>
#include <QVariant>

namespace mx {
class Token;
enum class TokenCategory : unsigned char;
}  // namespace
namespace mx::gui {

//! Interface for code view themes.
class ICodeViewTheme {
 public:
  struct ColorAndStyle final {
    QColor foreground_color;
    QColor background_color;
    bool bold{false};
    bool underline{false};
    bool strikeout{false};
    bool italic{false};
  };

  ICodeViewTheme(void) = default;

  virtual ~ICodeViewTheme(void);

  virtual QFont Font(void) const = 0;

  virtual QColor GutterForegroundColor(void) const = 0;
  virtual QColor GutterBackgroundColor(void) const = 0;

  virtual QColor DefaultForegroundColor(void) const = 0;
  virtual QColor DefaultBackgroundColor(void) const = 0;

  virtual QColor CurrentLineBackgroundColor(void) const = 0;
  virtual QColor CurrentEntityBackgroundColor(void) const = 0;

  virtual ColorAndStyle TokenColorAndStyle(const Token &) const = 0;
};

//! The reference explorer widget
class ThemeManager : public QObject {
  Q_OBJECT

  ThemeManager(void) = delete;

 public:
  explicit ThemeManager(QApplication &application);

  //! Initialization method
  static void Initialize(QApplication &application);

  //! Returns an instance of the ThemeManager
  static ThemeManager &Get(void);

  //! Sets the active theme
  virtual void SetTheme(const bool &dark) = 0;

  //! Returns the active QPalette
  virtual const QPalette &Palette(void) const = 0;

  //! Returns the active CodeViewTheme
  virtual const ICodeViewTheme &CodeViewTheme(void) const = 0;

  //! Sends a ThemeChanged update to all connected components
  virtual void SendGlobalUpdate(void) const = 0;

  //! Returns true if the active theme is dark
  virtual bool isDarkTheme(void) const = 0;

  //! Constructor
  ThemeManager(void) = default;

  //! Destructor
  virtual ~ThemeManager(void) override = default;

  ThemeManager(const ThemeManager &) = delete;
  ThemeManager(ThemeManager &&) = delete;
  ThemeManager &operator=(const ThemeManager &) = delete;
  ThemeManager &operator=(ThemeManager &&) noexcept = delete;

 signals:
  //! Emitted when the theme has changed. Anyone who is listening to this
  //! signal should hold a reference to the `ThemeManager`, so that upon
  //! receipt of the signal, they can ask for the new pallette and code view
  //! theme.
  void ThemeChanged(void) const;
};

}  // namespace mx::gui
