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

QString GetLocationString(const QModelIndex &index) {
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

  return location;
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

  const auto &label = label_var.toString();
  auto location = GetLocationString(index);

  QFontMetrics font_metrics(option.font);
  auto text_width = std::max(font_metrics.horizontalAdvance(label),
                             font_metrics.horizontalAdvance(location));

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
  auto icon_bg_var = index.data(ReferenceExplorerModel::ExpansionModeColor);
  if (icon_bg_var.isValid()) {
    icon_bg = qvariant_cast<QColor>(icon_bg_var);
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

  painter->translate(option.rect.x() + margin, option.rect.y() + margin);

  DrawIcon(*painter, icon_size, icon_label, icon_bg);

  painter->translate(icon_size + (margin * 2), 0);

  auto rect_width{option.rect.width() - icon_size - (margin * 2)};

  auto font = option.font;
  font.setBold(true);
  painter->setFont(font);

  painter->setPen(option.palette.windowText().color());
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

  pen_color = pen_color.lighter(50);
  painter.setPen(QColor(255, 255, 255));
  painter.drawText(QRect(0, 0, size, size), Qt::AlignCenter, text);
}

bool ReferenceExplorerItemDelegate::editorEvent(QEvent *, QAbstractItemModel *,
                                                const QStyleOptionViewItem &,
                                                const QModelIndex &) {
  return false;
}

}  // namespace mx::gui
