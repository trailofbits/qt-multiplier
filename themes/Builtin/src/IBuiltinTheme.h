/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/Frontend/Token.h>
#include <multiplier/Frontend/TokenCategory.h>
#include <multiplier/GUI/Managers/ThemeManager.h>

namespace mx::gui {

class IBuiltinTheme : public ITheme {
  Q_OBJECT

 public:

  static constexpr size_t kNumTokenCategories = NumEnumerators(TokenCategory{});

  //! This structure represents a theme for the ICodeView widget
  struct ThemeData final {
    QColor selected_line_background_color;
    QColor highlighted_entity_background_color;

    QColor default_background_color;
    QColor default_foreground_color;

    QColor default_gutter_background;
    QColor default_gutter_foreground;

    ITheme::ColorAndStyle token_styles[kNumTokenCategories];
  };

  QFont font;
  const QString id;
  const QString name;
  const QPalette palette;
  const ThemeData &data;

  virtual ~IBuiltinTheme(void);

  IBuiltinTheme(MediaManager &media, QString name_, QString id_,
                QPalette palette_, const ThemeData &data_);

  QString Name(void) const Q_DECL_FINAL;
  QString Id(void) const Q_DECL_FINAL;
  QFont Font(void) const Q_DECL_FINAL;
  QColor GutterForegroundColor(void) const Q_DECL_FINAL;
  QColor GutterBackgroundColor(void) const Q_DECL_FINAL;
  QColor DefaultForegroundColor(void) const Q_DECL_FINAL;
  QColor DefaultBackgroundColor(void) const Q_DECL_FINAL;
  QColor CurrentLineBackgroundColor(void) const Q_DECL_FINAL;
  QColor CurrentEntityBackgroundColor(void) const Q_DECL_FINAL;
  ColorAndStyle TokenColorAndStyle(const Token &token) const Q_DECL_FINAL;
};

}  // namespace mx::gui
