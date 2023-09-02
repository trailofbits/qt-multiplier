/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/ui/Icons.h>
#include <multiplier/ui/IThemeManager.h>

#include <QBitmap>
#include <QPainter>
#include <QColor>

namespace mx::gui {

namespace {

QPixmap GetColorizedPixmap(const QString &path, const QColor &color) {
  QPixmap pixmap(path);

  QPainter painter(&pixmap);

  auto mask = pixmap.createMaskFromColor(Qt::transparent, Qt::MaskInColor);
  painter.setClipRegion(mask);

  painter.setCompositionMode(QPainter::CompositionMode_Multiply);
  painter.fillRect(QRect(QPoint(0, 0), pixmap.size()), color);

  return pixmap;
}

QColor GetIconColor(const IconStyle &style) {
  auto is_dark_theme = IThemeManager::Get().isDarkTheme();

  QColor color;
  switch (style) {
    case IconStyle::None: color = is_dark_theme ? Qt::white : Qt::black; break;

    case IconStyle::Highlighted:
      color = is_dark_theme ? Qt::darkGray : Qt::red;
      break;

    case IconStyle::Disabled:
      color = is_dark_theme ? Qt::black : Qt::white;
      break;
  }

  return color;
}

}  // namespace

QIcon GetIcon(const QString &path, const IconStyle &style) {
  return QIcon(GetPixmap(path, style));
}

QPixmap GetPixmap(const QString &path, const IconStyle &style) {
  auto color = GetIconColor(style);
  return GetColorizedPixmap(path, color);
}

}  // namespace mx::gui
