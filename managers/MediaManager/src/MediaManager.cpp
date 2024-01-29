/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/GUI/Managers/MediaManager.h>

#include <QBitmap>
#include <QFontDatabase>
#include <QPainter>
#include <QColor>

#include <multiplier/GUI/Managers/ThemeManager.h>

namespace mx::gui {
namespace {

static const char *kFontList[] = {
  ":com.trailofbits.font.SourceCodePro-Black",
  ":com.trailofbits.font.SourceCodePro-BlackItalic",
  ":com.trailofbits.font.SourceCodePro-Bold",
  ":com.trailofbits.font.SourceCodePro-BoldItalic",
  ":com.trailofbits.font.SourceCodePro-ExtraBold",
  ":com.trailofbits.font.SourceCodePro-ExtraBoldItalic",
  ":com.trailofbits.font.SourceCodePro-ExtraLight",
  ":com.trailofbits.font.SourceCodePro-ExtraLightItalic",
  ":com.trailofbits.font.SourceCodePro-Italic",
  ":com.trailofbits.font.SourceCodePro-Light",
  ":com.trailofbits.font.SourceCodePro-LightItalic",
  ":com.trailofbits.font.SourceCodePro-Medium",
  ":com.trailofbits.font.SourceCodePro-MediumItalic",
  ":com.trailofbits.font.SourceCodePro-Regular",
  ":com.trailofbits.font.SourceCodePro-SemiBold",
  ":com.trailofbits.font.SourceCodePro-SemiBoldItalic",
};

static void InitializeFontDatabase(void) {
  for (auto font_path : kFontList) {
    (void) QFontDatabase::addApplicationFont(font_path);
  }
}

static QPixmap GetColorizedPixmap(const QString &path, const QColor &color) {
  QPixmap pixmap(path);
  auto mask = pixmap.createMaskFromColor(Qt::transparent, Qt::MaskInColor);

  QPainter painter(&pixmap);
  painter.setClipRegion(mask);
  painter.setCompositionMode(QPainter::CompositionMode_Multiply);
  painter.fillRect(QRect(QPoint(0, 0), pixmap.size()), color);

  return pixmap;
}

}  // namespace

class MediaManagerImpl {
 public:
  IThemePtr theme;

  inline MediaManagerImpl(ThemeManager &theme_manager)
      : theme(theme_manager.Theme()) {
    InitializeFontDatabase();    
  }
};

MediaManager::~MediaManager(void) {}

MediaManager::MediaManager(ThemeManager &theme_manager, QObject *parent)
    : QObject(parent),
      d(std::make_shared<MediaManagerImpl>(theme_manager)) {
  
  connect(&theme_manager, &ThemeManager::ThemeChanged,
          this, &MediaManager::OnThemeChanged);      
}

// TODO(pag): Could technically be a race condition where the theme manager
//            emits a `ThemeChanged` signal, but some other thing sees it first
//            and asks the media manager for an icon using the stale theme.
void MediaManager::OnThemeChanged(const ThemeManager &theme_manager) {
  d->theme = theme_manager.Theme();
  emit IconsChanged(*this);
}

QFont MediaManager::Font(const QString &id) const {
  return QFont(id);
}

QIcon MediaManager::Icon(const QString &id, ITheme::IconStyle style) const {
  return QIcon(Pixmap(id, style));
}

QPixmap MediaManager::Pixmap(const QString &id, ITheme::IconStyle style) const {
  return GetColorizedPixmap(":" + id, d->theme->IconColor(style));
}

}  // namespace mx::gui
