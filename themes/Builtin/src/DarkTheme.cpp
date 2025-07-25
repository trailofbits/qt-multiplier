/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/GUI/Managers/MediaManager.h>

#include "IBuiltinTheme.h"

#ifdef __APPLE__
#  include "MacosUtils.h"
#endif

namespace mx::gui {
namespace {

static QPalette GetDarkPalette(void) {
  QPalette palette;

  palette.setColor(QPalette::WindowText, QColor::fromRgb(0xffeeeeec));
  palette.setColor(QPalette::Button, QColor::fromRgb(0xff373737));
  palette.setColor(QPalette::Light, QColor::fromRgb(0xff515151));
  palette.setColor(QPalette::Midlight, QColor::fromRgb(0xff444444));
  palette.setColor(QPalette::Dark, QColor::fromRgb(0xff1e1e1e));
  palette.setColor(QPalette::Mid, QColor::fromRgb(0xff2a2a2a));
  palette.setColor(QPalette::Text, QColor::fromRgb(0xffeeeeec));
  palette.setColor(QPalette::BrightText, QColor::fromRgb(0xffffffff));
  palette.setColor(QPalette::ButtonText, QColor::fromRgb(0xffeeeeec));
  palette.setColor(QPalette::Base, QColor::fromRgb(0x1e, 0x1e, 0x1e).darker());
  palette.setColor(QPalette::Window, QColor::fromRgb(0xff353535));
  palette.setColor(QPalette::Shadow, QColor::fromRgb(0xff000000));
  palette.setColor(QPalette::Highlight, QColor::fromRgb(0xff15539e));
  palette.setColor(QPalette::HighlightedText, QColor::fromRgb(0xffffffff));
  palette.setColor(QPalette::Link, QColor::fromRgb(0xff3584e4));
  palette.setColor(QPalette::LinkVisited, QColor::fromRgb(0xff1b6acb));
  palette.setColor(QPalette::AlternateBase, QColor::fromRgb(0xff2d2d2d));
  palette.setColor(QPalette::NoRole, QColor::fromRgb(0xff000000));
  palette.setColor(QPalette::ToolTipBase, QColor::fromRgb(0xff262626));
  palette.setColor(QPalette::ToolTipText, QColor::fromRgb(0xffffffff));
  palette.setColor(QPalette::PlaceholderText, QColor::fromRgb(0xffeeeeec));

  return palette;
}

#define TCI(x) int(TokenCategory::x)

// Default text color for the dark theme.
static const QColor kDefaultDarkForegroundColor
    = QColor::fromRgb(255, 255, 255);

// Default background color for the dark theme.
static const QColor kDefaultDarkBackgroundColor
    = QColor::fromRgb(0x1e, 0x1e, 0x1e).darker();

static const QColor kCursorColor = QColor::fromRgb(255, 170, 66);

static const IBuiltinTheme::ThemeData kDarkThemeData{
  // selected_line_background_color
  QColor::fromRgb(0x1e, 0x1e, 0x1e),

  // highlighted_entity_background_color
  QColor::fromRgb(34, 48, 66),

  // default_background_color
  kDefaultDarkBackgroundColor,

  // default_foreground_color
  kDefaultDarkForegroundColor,

  // default_gutter_background
  QColor::fromRgb(0x1e, 0x1e, 0x1e).darker(),

  // default_gutter_foreground
  QColor::fromRgb(128, 128, 128),

  // token_styles
  {
    [TCI(UNKNOWN)] = {QColor::fromRgb(28, 1, 4), {}, false, false, false, false},
    [TCI(IDENTIFIER)] = {QColor::fromRgb(114, 114, 114), {}, false, false, false, false},
    [TCI(MACRO_NAME)] = {QColor::fromRgb(121, 244, 241), {}, false, false, false, false},
    [TCI(MACRO_PARAMETER_NAME)] = {QColor::fromRgb(114, 111, 58), {}, false, false, false, false},
    [TCI(MACRO_DIRECTIVE_NAME)] = {QColor::fromRgb(114, 111, 58), {}, true, false, false, false},
    [TCI(KEYWORD)] = {QColor::fromRgb(181, 116, 122), {}, false, false, false, false},
    [TCI(OBJECTIVE_C_KEYWORD)] = {QColor::fromRgb(181, 116, 122), {}, false, false, false, false},
    [TCI(BUILTIN_TYPE_NAME)] = {QColor::fromRgb(115, 61, 60), {}, false, false, false, false},
    [TCI(PUNCTUATION)] = {QColor::fromRgb(93, 93, 93), {}, false, false, false, false},
    [TCI(LITERAL)] = {QColor::fromRgb(226, 211, 148), {}, false, false, false, false},
    [TCI(COMMENT)] = {QColor::fromRgb(105, 104, 97), {}, false, false, false, false},
    [TCI(LOCAL_VARIABLE)] = {QColor::fromRgb(198, 125, 237), {}, false, false, false, false},
    [TCI(GLOBAL_VARIABLE)] = {QColor::fromRgb(198, 163, 73), {}, true, false, false, true},
    [TCI(PARAMETER_VARIABLE)] = {QColor::fromRgb(172, 122, 180), {}, false, false, false, false},
    [TCI(FUNCTION)] = {QColor::fromRgb(126, 125, 186), {}, true, false, false, false},
    [TCI(INSTANCE_METHOD)] = {QColor::fromRgb(126, 125, 186), {}, true, false, false, false},
    [TCI(INSTANCE_MEMBER)] = {QColor::fromRgb(207, 130, 235), {}, false, false, false, false},
    [TCI(CLASS_METHOD)] = {QColor::fromRgb(170, 129, 52), {}, true, false, false, true},
    [TCI(CLASS_MEMBER)] = {QColor::fromRgb(170, 129, 52), {}, false, false, false, true},
    [TCI(THIS)] = {QColor::fromRgb(181, 116, 122), {}, false, false, false, false},
    [TCI(CLASS)] = {QColor::fromRgb(0, 177, 110), {}, true, false, false, false},
    [TCI(STRUCT)] = {QColor::fromRgb(0, 177, 110), {}, true, false, false, false},
    [TCI(UNION)] = {QColor::fromRgb(0, 177, 110), {}, true, false, false, false},
    [TCI(CONCEPT)] = {QColor::fromRgb(0, 177, 110), {}, true, false, false, false},
    [TCI(INTERFACE)] = {QColor::fromRgb(0, 177, 110), {}, true, false, false, false},
    [TCI(ENUM)] = {QColor::fromRgb(175, 144, 65), {}, false, false, false, false},
    [TCI(ENUMERATOR)] = {QColor::fromRgb(113, 163, 98), {}, false, false, false, true},
    [TCI(NAMESPACE)] = {QColor::fromRgb(95, 154, 160), {}, false, false, false, false},
    [TCI(TYPE_ALIAS)] = {QColor::fromRgb(3, 171, 108), {}, true, false, false, false},
    [TCI(TEMPLATE_PARAMETER_TYPE)] = {QColor::fromRgb(198, 117, 29), {}, false, false, false, false},
    [TCI(TEMPLATE_PARAMETER_VALUE)] = {QColor::fromRgb(174, 144, 65), {}, false, false, false, true},
    [TCI(LABEL)] = {QColor::fromRgb(149, 149, 149), {}, false, false, false, false},
    [TCI(WHITESPACE)] = {QColor::fromRgb(93, 93, 93), {}, false, false, false, false},
    [TCI(FILE_NAME)] = {QColor::fromRgb(23, 185, 152), {}, false, false, false, false},
    [TCI(LINE_NUMBER)] = {QColor::fromRgb(109, 132, 140), {}, false, false, false, false},
    [TCI(COLUMN_NUMBER)] = {QColor::fromRgb(109, 132, 140), {}, false, false, false, false},
    [TCI(INFORMATION)] = {QColor::fromRgb(105, 104, 97), {}, false, false, false, false},
    [TCI(WARNING)] = {QColor::fromRgb(105, 104, 97), {}, false, false, false, false},
    [TCI(ERROR)] = {QColor::fromRgb(105, 104, 97), {}, false, false, false, false},
  },
};

class DarkTheme Q_DECL_FINAL : public IBuiltinTheme {
  Q_OBJECT

 public:
  virtual ~DarkTheme(void) = default;

  explicit DarkTheme(const MediaManager &media)
      : IBuiltinTheme(media, tr("Dark"), "com.trailofbits.theme.Dark",
                      GetDarkPalette(), kDarkThemeData) {}

  void Apply(QApplication &) Q_DECL_FINAL {
#ifdef __APPLE__
    SetNSAppToDarkTheme();
#else
    qApp->setPalette(Palette());
#endif
  }

  //! Color used by the cursor.
  QColor CursorColor(void) const Q_DECL_FINAL {
    return kCursorColor;
  }

  QColor SelectionColor(void) const Q_DECL_FINAL {
    return QColor::fromRgb(0x1e, 0x1e, 0x1e).lighter(200);
  }

  QColor IconColor(IconStyle style) const Q_DECL_FINAL {
    switch (style) {
      case IconStyle::NONE: return Qt::lightGray;
      case IconStyle::HIGHLIGHTED: return Qt::white;
      case IconStyle::DISABLED: return Qt::darkGray;
    }
  }
};

}  // namespace

std::unique_ptr<ITheme> CreateDarkTheme(const MediaManager &media) {
  return std::make_unique<DarkTheme>(media);
}

}  // namespace mx::gui

#include "DarkTheme.moc"
