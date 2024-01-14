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

static QPalette GetLightPalette(void) {
  QPalette palette;

  palette.setColor(QPalette::WindowText, QColor::fromRgb(0xff2e3436));
  palette.setColor(QPalette::Button, QColor::fromRgb(0xfff6f5f4));
  palette.setColor(QPalette::Light, QColor::fromRgb(0xffffffff));
  palette.setColor(QPalette::Midlight, QColor::fromRgb(0xfffbfafa));
  palette.setColor(QPalette::Dark, QColor::fromRgb(0xffdfdcd8));
  palette.setColor(QPalette::Mid, QColor::fromRgb(0xffebe8e6));
  palette.setColor(QPalette::Text, QColor::fromRgb(0xff2e3436));
  palette.setColor(QPalette::BrightText, QColor::fromRgb(0xff000000));
  palette.setColor(QPalette::ButtonText, QColor::fromRgb(0xff2e3436));
  palette.setColor(QPalette::Base, QColor::fromRgb(255, 255, 255));
  palette.setColor(QPalette::Window, QColor::fromRgb(0xfff6f5f4));
  palette.setColor(QPalette::Shadow, QColor::fromRgb(0xff000000));
  palette.setColor(QPalette::Highlight, QColor::fromRgb(0xff3584e4));
  palette.setColor(QPalette::HighlightedText, QColor::fromRgb(0xffffffff));
  palette.setColor(QPalette::Link, QColor::fromRgb(0xff1b6acb));
  palette.setColor(QPalette::LinkVisited, QColor::fromRgb(0xff15539e));
  palette.setColor(QPalette::AlternateBase, QColor::fromRgb(0xffffffff));
  palette.setColor(QPalette::NoRole, QColor::fromRgb(0xff000000));
  palette.setColor(QPalette::ToolTipBase, QColor::fromRgb(0xff353535));
  palette.setColor(QPalette::ToolTipText, QColor::fromRgb(0xffffffff));
  palette.setColor(QPalette::PlaceholderText, QColor::fromRgb(0xff2e3436));

  return palette;
}

#define TCI(x) int(TokenCategory::x)

// Default text color for the light theme.
static const QColor kDefaultLightForegroundColor = QColor::fromRgb(34, 34, 34);

// Default background color for the light theme.
static const QColor kDefaultLightBackgroundColor
    = QColor::fromRgb(255, 255, 255);

static const QColor kCursorColor = QColor::fromRgb(0x1e, 0x1e, 0x1e);

static const IBuiltinTheme::ThemeData kLightThemeData{
  // selected_line_background_color
  QColor::fromRgb(236, 236, 236),

  // highlighted_entity_background_color
  QColor::fromRgb(204, 204, 255),

  // default_background_color
  kDefaultLightBackgroundColor,

  // default_foreground_color
  kDefaultLightForegroundColor,

  // default_gutter_background
  QColor::fromRgb(160, 160, 160),

  // default_gutter_foreground
  QColor::fromRgb(0, 0, 0),

  // token_styles
  {
    [TCI(UNKNOWN)] = {QColor::fromRgb(2, 2, 2), {}, false, false, false, false},
    [TCI(IDENTIFIER)] = {QColor::fromRgb(19, 19, 19), {}, false, false, false, false},
    [TCI(MACRO_NAME)] = {QColor::fromRgb(128, 0, 128), {}, false, false, false, false},
    [TCI(MACRO_PARAMETER_NAME)] = {QColor::fromRgb(0, 0, 0), {}, false, false, false, false},
    [TCI(MACRO_DIRECTIVE_NAME)] = {QColor::fromRgb(0, 128, 0), {}, true, false, false, false},
    [TCI(KEYWORD)] = {QColor::fromRgb(0, 0, 128), {}, false, false, false, false},
    [TCI(OBJECTIVE_C_KEYWORD)] = {QColor::fromRgb(0, 0, 128), {}, false, false, false, false},
    [TCI(BUILTIN_TYPE_NAME)] = {QColor::fromRgb(33, 33, 144), {}, false, false, false, false},
    [TCI(PUNCTUATION)] = {QColor::fromRgb(51, 51, 51), {}, false, false, false, false},
    [TCI(LITERAL)] = {QColor::fromRgb(0, 128, 128), {}, false, false, false, false},
    [TCI(COMMENT)] = {QColor::fromRgb(0, 0, 255), {}, false, false, false, true},
    [TCI(LOCAL_VARIABLE)] = {QColor::fromRgb(0, 51, 102), {}, false, false, false, false},
    [TCI(GLOBAL_VARIABLE)] = {QColor::fromRgb(0, 51, 102), {}, true, false, false, true},
    [TCI(PARAMETER_VARIABLE)] = {QColor::fromRgb(0, 77, 102), {}, false, false, false, false},
    [TCI(FUNCTION)] = {QColor::fromRgb(128, 0, 0), {}, true, false, false, false},
    [TCI(INSTANCE_METHOD)] = {QColor::fromRgb(142, 28, 28), {}, true, false, false, true},
    [TCI(INSTANCE_MEMBER)] = {QColor::fromRgb(0, 51, 102), {}, false, false, false, true},
    [TCI(CLASS_METHOD)] = {QColor::fromRgb(128, 0, 0), {}, false, false, false, true},
    [TCI(CLASS_MEMBER)] = {QColor::fromRgb(0, 51, 102), {}, false, false, false, true},
    [TCI(THIS)] = {QColor::fromRgb(0, 0, 128), {}, true, false, false, false},
    [TCI(CLASS)] = {QColor::fromRgb(0, 0, 0), {}, false, true, false, false},
    [TCI(STRUCT)] = {QColor::fromRgb(0, 0, 0), {}, false, true, false, false},
    [TCI(UNION)] = {QColor::fromRgb(0, 0, 0), {}, false, true, false, false},
    [TCI(CONCEPT)] = {QColor::fromRgb(0, 0, 0), {}, false, true, false, false},
    [TCI(INTERFACE)] = {QColor::fromRgb(0, 0, 0), {}, false, true, false, false},
    [TCI(ENUM)] = {QColor::fromRgb(0, 0, 0), {}, false, true, false, false},
    [TCI(ENUMERATOR)] = {QColor::fromRgb(0, 128, 255), {}, false, false, false, true},
    [TCI(NAMESPACE)] = {QColor::fromRgb(3, 3, 3), {}, false, false, false, false},
    [TCI(TYPE_ALIAS)] = {QColor::fromRgb(0, 0, 0), {}, false, true, false, false},
    [TCI(TEMPLATE_PARAMETER_TYPE)] = {QColor::fromRgb(0, 0, 0), {}, false, true, false, false},
    [TCI(TEMPLATE_PARAMETER_VALUE)] = {QColor::fromRgb(0, 128, 128), {}, false, false, false, true},
    [TCI(LABEL)] = {QColor::fromRgb(16, 16, 16), {}, false, false, false, false},
    [TCI(WHITESPACE)] = {QColor::fromRgb(51, 51, 51), {}, false, false, false, false},
    [TCI(FILE_NAME)] = {QColor::fromRgb(206, 18, 18), {}, false, false, false, false},
    [TCI(LINE_NUMBER)] = {QColor::fromRgb(0, 0, 0), {}, false, false, false, false},
    [TCI(COLUMN_NUMBER)] = {QColor::fromRgb(0, 0, 0), {}, false, false, false, false},
  },
};

class LightTheme Q_DECL_FINAL : public IBuiltinTheme {
  Q_OBJECT

 public:
  virtual ~LightTheme(void) = default;

  explicit LightTheme(const MediaManager &media)
      : IBuiltinTheme(media, tr("Light"), "com.trailofbits.theme.Light",
                      GetLightPalette(), kLightThemeData) {}

  void Apply(QApplication &) Q_DECL_FINAL {
#ifdef __APPLE__
    SetNSAppToLightTheme();
#endif
  }

  QColor CursorColor(void) const Q_DECL_FINAL {
    return kCursorColor;
  }

  QColor IconColor(IconStyle style) const Q_DECL_FINAL {
    switch (style) {
      case IconStyle::NONE: return Qt::darkGray;
      case IconStyle::HIGHLIGHTED: return Qt::black;
      case IconStyle::DISABLED: return Qt::lightGray;
    }
  }
};

}  // namespace

std::unique_ptr<ITheme> CreateLightTheme(const MediaManager &media) {
  return std::make_unique<LightTheme>(media);
}

}  // namespace mx::gui

#include "LightTheme.moc"
