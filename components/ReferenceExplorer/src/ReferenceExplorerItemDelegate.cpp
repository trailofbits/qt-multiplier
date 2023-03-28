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
  return font_metrics.height() / 3;
}

int GetIconSize(const QFontMetrics &font_metrics) {
  return font_metrics.height() * 2;
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
  auto margin{GetMarginSize(font_metrics)};
  auto icon_size{GetIconSize(font_metrics)};

  const auto &label = label_var.toString();
  auto required_width{margin + icon_size + margin +
                      font_metrics.horizontalAdvance(label) + margin};
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

  QString location;
  QVariant location_info_var =
      index.data(IReferenceExplorerModel::LocationRole);

  if (location_info_var.isValid()) {
    auto location_info = qvariant_cast<Location>(location_info_var);
    auto filename =
        std::filesystem::path(location_info.path.toStdString()).filename();

    location = QString::fromStdString(filename);

    if (0u < location_info.line) {
      location += ":" + QString::number(location_info.line);

      if (0u < location_info.column) {
        location += ":" + QString::number(location_info.column);
      }
    }
  }

  QString icon_label;
  auto icon_label_var = index.data(ReferenceExplorerModel::IconLabelRole);
  if (icon_label_var.isValid()) {
    icon_label = icon_label_var.toString();
  } else {
    icon_label = "?";
  }

  QFontMetrics font_metrics(option.font);
  auto margin{GetMarginSize(font_metrics)};
  auto icon_size{GetIconSize(font_metrics)};
  auto font_height{font_metrics.height()};

  painter->save();

  painter->setRenderHint(QPainter::Antialiasing, true);
  if ((option.state & QStyle::State_Selected) != 0) {
    painter->fillRect(option.rect, option.palette.highlight());
  }

  int translation{option.rect.x() + margin};
  painter->translate(translation, option.rect.y() + margin);

  DrawIcon(*painter, icon_size, icon_label);

  translation += icon_size + margin;
  painter->translate(icon_size + margin, 0);

  auto rect_width{option.rect.width() - translation};

  auto font = option.font;
  font.setBold(true);
  painter->setFont(font);

  painter->setPen(option.palette.windowText().color());
  painter->drawText(QRect(0, 0, rect_width, font_height), Qt::AlignVCenter,
                    label);

  font.setBold(false);
  painter->setFont(font);

  painter->setPen(option.palette.dark().color());
  painter->drawText(QRect(0, icon_size - font_height, rect_width, font_height),
                    Qt::AlignVCenter, location);

  painter->restore();
}

void ReferenceExplorerItemDelegate::DrawIcon(QPainter &painter, const int &size,
                                             const QString &text) {

  painter.fillRect(0, 0, size, size, QColor::fromRgb(0x40, 0x40, 0x40));

  auto pen_color{QColor::fromRgb(0, 0, 0)};
  painter.setPen(pen_color);
  painter.drawRect(0, 0, size, size);

  pen_color = pen_color.lighter(50);
  painter.setPen(pen_color);
  painter.drawText(QRect(0, 0, size, size), Qt::AlignCenter, text);
}

bool ReferenceExplorerItemDelegate::editorEvent(QEvent *, QAbstractItemModel *,
                                                const QStyleOptionViewItem &,
                                                const QModelIndex &) {
  return false;
}

}  // namespace mx::gui
