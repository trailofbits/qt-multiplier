/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "ReferenceExplorerItemDelegate.h"
#include "ReferenceExplorerModel.h"

#include <multiplier/ui/IReferenceExplorerModel.h>

#include <QFontMetrics>
#include <QPainter>

#include <filesystem>

#include "Types.h"

namespace mx::gui {

namespace {

int GetMarginSize(const QFontMetrics &font_metrics) {
  return font_metrics.height() / 4;
}

int GetIconSize(const QFontMetrics &font_metrics) {
  return static_cast<int>(font_metrics.height() * 1.5);
}

}  // namespace

struct ReferenceExplorerItemDelegate::PrivateData final {};

ReferenceExplorerItemDelegate::ReferenceExplorerItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent),
      d(new PrivateData) {}

ReferenceExplorerItemDelegate::~ReferenceExplorerItemDelegate() {}

QSize ReferenceExplorerItemDelegate::sizeHint(
    const QStyleOptionViewItem &option, const QModelIndex &index) const {

  auto label_var = index.data();
  if (!label_var.isValid()) {
    return QStyledItemDelegate::sizeHint(option, index);
  }

  QFontMetrics font_metrics(option.font);
  auto text_width = font_metrics.horizontalAdvance(label_var.toString());

  auto margin{GetMarginSize(font_metrics)};
  auto icon_size{GetIconSize(font_metrics)};

  auto required_width{margin + icon_size + margin + text_width + margin};
  auto required_height{margin + icon_size + margin};

  return QSize(required_width, required_height);
}

void ReferenceExplorerItemDelegate::paint(QPainter *painter,
                                          const QStyleOptionViewItem &option,
                                          const QModelIndex &index) const {

  auto label_var = index.data();
  if (!label_var.isValid()) {
    QStyledItemDelegate::paint(painter, option, index);
    return;
  }

  auto label = label_var.toString();

  QString icon_label = "Unk";
  auto icon_label_var = index.data(ReferenceExplorerModel::IconLabelRole);
  if (icon_label_var.isValid()) {
    icon_label = icon_label_var.toString();
  }

  QColor icon_bg = option.palette.base().color().darker();

  QFontMetrics font_metrics(option.font);
  auto margin{GetMarginSize(font_metrics)};
  auto icon_size{GetIconSize(font_metrics)};
  auto font_height{font_metrics.height()};

  painter->save();

  painter->setRenderHint(QPainter::Antialiasing, true);

  auto background_color{option.palette.base()};
  if ((option.state & QStyle::State_Selected) != 0) {
    background_color = option.palette.highlight();

  } else if (auto background_role_var = index.data(Qt::BackgroundRole);
             background_role_var.isValid()) {
    background_color = qvariant_cast<QColor>(background_role_var);
  }

  painter->fillRect(option.rect, background_color);

  painter->translate(option.rect.x() + margin, option.rect.y() + margin);

  DrawIcon(*painter, icon_size, icon_label, icon_bg);

  painter->translate(icon_size + (margin * 2), 0);

  auto rect_width{option.rect.width() - icon_size - (margin * 2)};

  auto font = option.font;
  font.setBold(true);
  painter->setFont(font);

  if (auto foreground_color_var = index.data(Qt::ForegroundRole);
      foreground_color_var.isValid()) {
    const auto &foreground_color = qvariant_cast<QColor>(foreground_color_var);
    painter->setPen(QPen(foreground_color));
    painter->setBrush(QBrush(foreground_color));

  } else {
    painter->setPen(option.palette.text().color());
  }

  painter->drawText(
      QRect(0, (icon_size / 2) - (font_height / 2), rect_width, font_height),
      Qt::AlignVCenter, label);

  painter->restore();
}

void ReferenceExplorerItemDelegate::DrawIcon(QPainter &painter, const int &size,
                                             const QString &text,
                                             const QColor &bg) {

  painter.fillRect(0, 0, size, size, bg);

  auto pen_color{QColor::fromRgb(0, 0, 0)};
  painter.setPen(pen_color);
  painter.drawRect(0, 0, size, size);

  auto text_font = painter.font();
  text_font.setPointSize(static_cast<int>(text_font.pointSize() * 0.8));

  pen_color = pen_color.lighter(50);
  painter.setPen(QColor(255, 255, 255));

  auto original_font = painter.font();
  painter.setFont(text_font);
  painter.drawText(QRect(0, 0, size, size), Qt::AlignCenter, text);

  painter.setFont(original_font);
}

bool ReferenceExplorerItemDelegate::editorEvent(QEvent *, QAbstractItemModel *,
                                                const QStyleOptionViewItem &,
                                                const QModelIndex &) {
  return false;
}

}  // namespace mx::gui
