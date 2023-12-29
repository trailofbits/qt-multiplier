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
#include <QString>

#include <memory>
#include <vector>

namespace mx {
class Token;
}  // namespace
namespace mx::gui {

//! Interface for themes.
class ITheme : public QObject {
  Q_OBJECT

 public:
  using Ptr = std::shared_ptr<const ITheme>;

  struct ColorAndStyle final {
    QColor foreground_color;
    QColor background_color;
    bool bold{false};
    bool underline{false};
    bool strikeout{false};
    bool italic{false};
  };

  //! Icon style
  enum class IconStyle {
    NONE,
    HIGHLIGHTED,
    DISABLED,
  };

  virtual ~ITheme(void);

  ITheme(void) = default;

  static std::unique_ptr<ITheme> CreateDarkTheme(void);
  static std::unique_ptr<ITheme> CreateLightTheme(void);

  //! Apply this theme to an application. This is invoked when the theme manager
  //! sets the theme, or when this is already the active theme but the theme
  //! itself changes, e.g. due to signalling `ThemeChanged`.
  virtual void Apply(QApplication &application);

  //! Returns the active QPalette
  virtual const QPalette &Palette(void) const = 0;

  //! Human-readable name for this theme.
  virtual QString Name(void) const = 0;

  //! Namespaced unique id for this theme, e.g. `com.trailofbits.theme.Dark`.
  virtual QString Id(void) const = 0;

  //! Font used by this theme.
  virtual QFont Font(void) const = 0;

  //! Color used for icons. Most icons are a single color with a transparent
  //! background, and represent a mask. This color is applied to the mask to
  //! make the theme-specific colored icon.
  virtual QColor IconColor(IconStyle style) const = 0;

  //! Foreground (text) color for line numbers and other text in the gutter.
  virtual QColor GutterForegroundColor(void) const = 0;

  //! Background color for the gutter.
  virtual QColor GutterBackgroundColor(void) const = 0;

  //! Default foreground (text) color for text in a code view.
  virtual QColor DefaultForegroundColor(void) const = 0;

  //! Default background color for a code view.
  virtual QColor DefaultBackgroundColor(void) const = 0;

  //! Background color for the current line, i.e. the line containing the user's
  //! cursor.
  virtual QColor CurrentLineBackgroundColor(void) const = 0;

  //! Background color for the current entity, i.e. when the cursor is on a
  //! token, and when the token has a related entity, then all tokens sharing
  //! the same related entity are highlighted with this color.
  virtual QColor CurrentEntityBackgroundColor(void) const = 0;

  //! The color and style applied to a given token.
  virtual ColorAndStyle TokenColorAndStyle(const Token &) const = 0;
 
 signals:
  // Emitted when this theme changes some of its own colors.
  void ThemeChanged(void);
};

//! Manages the themes.
class ThemeManager Q_DECL_FINAL : public QObject {
  Q_OBJECT

  struct PrivateData;
  const std::shared_ptr<PrivateData> d;

 public:
  virtual ~ThemeManager(void);

  explicit ThemeManager(QApplication &application);

  //! Register a theme with the manager. The first registered theme is set to
  //! be the current theme.
  void Register(std::unique_ptr<ITheme> theme);

  //! Sets the active theme. This is a no-op if `theme` is not owned by this
  //! theme manager.
  void SetTheme(ITheme::Ptr theme);

  //! Returns the active theme.
  ITheme::Ptr Theme(void) const;

  //! Look up a theme by its ID, e.g. `com.trailofbits.theme.Dark`.
  ITheme::Ptr Find(const QString &id) const;

  // Return the list of registered theme IDs.
  std::vector<ITheme::Ptr> ThemeList(void) const;

 signals:
  //! Emitted when the theme has changed. Sends out the new theme.
  void ThemeChanged(ITheme::Ptr);

  //! Emitted when the set of themes change. Anyone who is listening to this
  //! signal should hold a reference to the `ThemeManager`, so that upon
  //! receipt of the signal, they can ask for the new pallette and code view
  //! theme.
  void ThemeListChanged(const std::vector<ITheme::Ptr> &new_list);
};

}  // namespace mx::gui

Q_DECLARE_INTERFACE(mx::gui::ITheme,
                    "com.trailofbits.interface.ITheme")
