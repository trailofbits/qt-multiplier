/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
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
#include <multiplier/Entity.h>
#include <string>
#include <vector>

namespace mx::gui {

class ITheme;

using IThemePtr = std::shared_ptr<const ITheme>;

//! Interface for themes.
class ITheme : public QObject {
  Q_OBJECT

 public:

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

  using QObject::QObject;

  //! Apply this theme to an application. This is invoked when the theme manager
  //! sets the theme, or when this is already the active theme but the theme
  //! itself changes, e.g. due to signalling `ThemeChanged`.
  virtual void Apply(QApplication &application) = 0;

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
  virtual QColor CurrentEntityBackgroundColor(
      const VariantEntity &entity) const = 0;

  //! The color and style applied to a given token.
  virtual ColorAndStyle TokenColorAndStyle(const Token &) const = 0;

  //! The color applied to a cell/row/etc, where the `QModelIndex` for that
  //! cell/row/etc. has an entity associated entity. This is designed to provide
  //! the value of `Qt::BackgroundRole`.
  virtual std::optional<QColor> EntityBackgroundColor(
      const VariantEntity &entity) const;

  //! Helper to compute a high-contrast foreground color given a background
  //! color.
  static QColor ContrastingColor(const QColor &background_color);
 
 signals:
  // Emitted when this theme changes some of its own colors.
  void ThemeChanged(void);
};

}  // namespace mx::gui

Q_DECLARE_INTERFACE(mx::gui::ITheme,
                    "com.trailofbits.interface.ITheme")
