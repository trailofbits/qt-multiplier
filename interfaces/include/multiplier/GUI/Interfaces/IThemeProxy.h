/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include "ITheme.h"

namespace mx::gui {

class IThemeProxy;

using IThemeProxyPtr = std::unique_ptr<IThemeProxy>;

//! A theme proxy allows one to implement theme-level changes without modifying
//! an existing theme. For example, they can be used to enact global changes
//! to the highlighting of specific entities.
class IThemeProxy : public QObject {
  Q_OBJECT

 public:
  virtual ~IThemeProxy(void);

  //! Uninstall this proxy from the theme manager that owns it.
  void UninstallFromOwningManager(void);

  //! Font used by this theme.
  virtual QFont Font(QFont theme_font) const;

  //! Color used by the cursor.
  virtual QColor CursorColor(QColor theme_color) const;

  //! Color used for icons. Most icons are a single color with a transparent
  //! background, and represent a mask. This color is applied to the mask to
  //! make the theme-specific colored icon.
  virtual QColor IconColor(QColor theme_color, ITheme::IconStyle style) const;

  //! Foreground (text) color for line numbers and other text in the gutter.
  virtual QColor GutterForegroundColor(QColor theme_color) const;

  //! Background color for the gutter.
  virtual QColor GutterBackgroundColor(QColor theme_color) const;

  //! Default foreground (text) color for text in a code view.
  virtual QColor DefaultForegroundColor(QColor theme_color) const;

  //! Default background color for a code view.
  virtual QColor DefaultBackgroundColor(QColor theme_color) const;

  //! Background color for the current line, i.e. the line containing the user's
  //! cursor.
  virtual QColor CurrentLineBackgroundColor(QColor theme_color) const;

  //! Background color for the current entity, i.e. when the cursor is on a
  //! token, and when the token has a related entity, then all tokens sharing
  //! the same related entity are highlighted with this color.
  virtual QColor CurrentEntityBackgroundColor(
      QColor theme_color, const VariantEntity &entity) const;

  //! The color and style applied to a given token.
  virtual ITheme::ColorAndStyle TokenColorAndStyle(
      ITheme::ColorAndStyle theme_color_and_style, const Token &token) const;

  //! The color applied to a cell/row/etc, where the `QModelIndex` for that
  //! cell/row/etc. has an entity associated entity. This is designed to provide
  //! the value of `Qt::BackgroundRole`.
  virtual std::optional<QColor> EntityBackgroundColor(
      std::optional<QColor> theme_color, const VariantEntity &entity) const;
 
 signals:
  //! Emitted when this theme proxy changes some of its own colors.
  void ThemeProxyChanged(void);

  //! Emitted when this theme proxy should be uninstalled.
  void Uninstall(IThemeProxy *self);
};

}  // namespace mx::gui

Q_DECLARE_INTERFACE(mx::gui::IThemeProxy,
                    "com.trailofbits.interface.IThemeProxy")
