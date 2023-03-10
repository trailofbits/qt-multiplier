/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "ReferenceExplorerItemDelegate.h"

#include <multiplier/ui/IReferenceExplorerModel.h>

#include <QFontMetrics>
#include <QPainter>

#include <filesystem>

#include "Types.h"

namespace mx::gui {

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

  QFontMetrics font_metrics(option.font);
  auto font_height = font_metrics.height();

  auto margin = font_height / 3;

  auto required_width = font_metrics.horizontalAdvance(label) + margin;
  auto required_height = (font_height * 2) + margin;

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
  QVariant location_info_var = index.data(IReferenceExplorerModel::LocationRole);
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

  painter->save();

  painter->setRenderHint(QPainter::Antialiasing, true);
  if ((option.state & QStyle::State_Selected) != 0) {
    painter->fillRect(option.rect, option.palette.highlight());
  }

  painter->translate(option.rect.x(), option.rect.y());

  QFontMetrics font_metrics(option.font);
  auto font_height = font_metrics.height();
  auto margin = font_height / 3;

  auto rect_height = option.rect.height();

  auto font = option.font;
  font.setBold(true);
  painter->setFont(font);

  painter->setPen(option.palette.windowText().color());
  painter->drawText(margin, font_height, label);

  font.setBold(false);
  painter->setFont(font);

  painter->setPen(option.palette.dark().color());
  painter->drawText(margin, rect_height - margin, location);

  painter->restore();
}

bool ReferenceExplorerItemDelegate::editorEvent(QEvent *, QAbstractItemModel *,
                                                const QStyleOptionViewItem &,
                                                const QModelIndex &) {
  return false;
}

}  // namespace mx::gui
