/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "EntityExplorerItemDelegate.h"

#include <multiplier/ui/CodeViewTheme.h>
#include <multiplier/ui/IEntityExplorerModel.h>

#include <multiplier/Token.h>

#include <QApplication>
#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QStyle>
#include <QStyleOptionViewItem>

#include <type_traits>

namespace mx::gui {

EntityExplorerItemDelegate::EntityExplorerItemDelegate(
    const CodeViewTheme &theme, QObject *parent)
    : QStyledItemDelegate(parent),
      d(TokenPainterConfiguration(theme)) {}

EntityExplorerItemDelegate::~EntityExplorerItemDelegate(void) {}

void EntityExplorerItemDelegate::SetTheme(const CodeViewTheme &theme) {
  TokenPainterConfiguration config = d.Configuration();
  config.theme = theme;
  d = TokenPainter(config);
}

void EntityExplorerItemDelegate::SetTabWidth(std::size_t width) {
  TokenPainterConfiguration config = d.Configuration();
  config.tab_width = width;
  d = TokenPainter(config);
}

void EntityExplorerItemDelegate::SetWhitespaceReplacement(QString data) {
  TokenPainterConfiguration config = d.Configuration();
  config.whitespace_replacement = data;
  d = TokenPainter(config);
}

void EntityExplorerItemDelegate::ClearWhitespaceReplacement(void) {
  TokenPainterConfiguration config = d.Configuration();
  config.whitespace_replacement.reset();
  d = TokenPainter(config);
}

void EntityExplorerItemDelegate::paint(QPainter *painter,
                                       const QStyleOptionViewItem &option,
                                       const QModelIndex &index) const {

  if (!index.isValid()) {
    this->QStyledItemDelegate::paint(painter, option, index);
    return;
  }

  QVariant val = index.data(IEntityExplorerModel::TokenRole);
  if (!val.isValid()) {
    this->QStyledItemDelegate::paint(painter, option, index);
    return;
  }

  auto code_view_theme = d.Configuration().theme;

  QColor background_color;
  if ((option.state & QStyle::State_Selected) != 0) {
    background_color = code_view_theme.selected_line_background_color;

  } else {
    if (auto background_var = index.data(Qt::BackgroundRole);
        background_var.isValid()) {
      background_color = background_var.value<QColor>();

    } else {
      background_color = code_view_theme.default_background_color;
    }
  }

  painter->fillRect(option.rect, QBrush(background_color));
  d.Paint(painter, option, qvariant_cast<Token>(val));

  // The highlight color used by the theme is barely visible, force better
  // highlighting using the standard highlight color to draw a frame
  // around the item
  if ((option.state & QStyle::State_Selected) != 0) {
    auto original_pen = painter->pen();
    painter->setPen(QPen(option.palette.highlight().color()));

    painter->drawRect(option.rect.adjusted(0, 0, -1, -1));
    painter->setPen(original_pen);
  }
}

QSize EntityExplorerItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                           const QModelIndex &index) const {
  if (!index.isValid()) {
    return this->QStyledItemDelegate::sizeHint(option, index);
  }

  QVariant val = index.data(IEntityExplorerModel::TokenRole);
  if (!val.isValid()) {
    return QStyledItemDelegate::sizeHint(option, index);
  }

  QSize contents_size = d.SizeHint(option, qvariant_cast<Token>(val));

  QStyleOptionViewItem opt(option);
  initStyleOption(&opt, index);
  QStyle *style = opt.widget ? opt.widget->style() : qApp->style();
  return style->sizeFromContents(QStyle::ContentsType::CT_ItemViewItem, &opt,
                                 contents_size, opt.widget);
}

}  // namespace mx::gui
