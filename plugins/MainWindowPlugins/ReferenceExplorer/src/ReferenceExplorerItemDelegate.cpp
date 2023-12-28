/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "ReferenceExplorerItemDelegate.h"

#include <multiplier/GUI/CodeViewTheme.h>
#include <multiplier/GUI/IGeneratorModel.h>
#include <multiplier/GUI/TokenPainter.h>

#include <multiplier/Frontend/Token.h>

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

struct ReferenceExplorerItemDelegate::PrivateData final {
  QFont font;
  std::unique_ptr<TokenPainter> token_painter;
};

ReferenceExplorerItemDelegate::ReferenceExplorerItemDelegate(
    const CodeViewTheme &theme, QObject *parent)
    : QStyledItemDelegate(parent),
      d(new PrivateData) {
  SetTheme(theme);
}

ReferenceExplorerItemDelegate::~ReferenceExplorerItemDelegate(void) {}

void ReferenceExplorerItemDelegate::SetTheme(const CodeViewTheme &theme) {
  d->font = QFont(theme.font_name);

  TokenPainterConfiguration token_painter_config({});
  if (d->token_painter != nullptr) {
    token_painter_config = d->token_painter->Configuration();
  }

  token_painter_config.theme = theme;

  d->token_painter = std::make_unique<TokenPainter>(token_painter_config);
}

void ReferenceExplorerItemDelegate::SetTabWidth(std::size_t width) {
  auto token_painter_config = d->token_painter->Configuration();
  token_painter_config.tab_width = width;

  d->token_painter = std::make_unique<TokenPainter>(token_painter_config);
}

void ReferenceExplorerItemDelegate::SetWhitespaceReplacement(QString data) {
  auto token_painter_config = d->token_painter->Configuration();
  token_painter_config.whitespace_replacement = data;

  d->token_painter = std::make_unique<TokenPainter>(token_painter_config);
}

void ReferenceExplorerItemDelegate::ClearWhitespaceReplacement(void) {
  auto token_painter_config = d->token_painter->Configuration();
  token_painter_config.whitespace_replacement.reset();

  d->token_painter = std::make_unique<TokenPainter>(token_painter_config);
}

void ReferenceExplorerItemDelegate::paint(QPainter *painter,
                                          const QStyleOptionViewItem &option,
                                          const QModelIndex &index) const {

  if (!index.isValid()) {
    this->QStyledItemDelegate::paint(painter, option, index);
    return;
  }

  // This item may have been highlighted by the global highlighter. If that
  // is the case, then do not apply any style and just forward this to
  // the base QStyledItemDelegate class
  if (auto background_var = index.data(Qt::BackgroundRole);
      background_var.isValid()) {
    QStyledItemDelegate::paint(painter, option, index);
    return;
  }

  QVariant val = index.data(IGeneratorModel::TokenRangeRole);
  if (!val.isValid()) {
    this->QStyledItemDelegate::paint(painter, option, index);
    return;
  }

  auto code_view_theme = d->token_painter->Configuration().theme;

  QColor background_color;
  if ((option.state & QStyle::State_Selected) != 0) {
    background_color = code_view_theme.selected_line_background_color;
  } else {
    background_color = code_view_theme.default_background_color;
  }

  painter->fillRect(option.rect, QBrush(background_color));
  d->token_painter->Paint(painter, option, qvariant_cast<TokenRange>(val));

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

QSize ReferenceExplorerItemDelegate::sizeHint(
    const QStyleOptionViewItem &option, const QModelIndex &index) const {
  if (!index.isValid()) {
    return this->QStyledItemDelegate::sizeHint(option, index);
  }

  QVariant val = index.data(IGeneratorModel::TokenRangeRole);
  if (!val.isValid()) {
    return QStyledItemDelegate::sizeHint(option, index);
  }

  QSize contents_size =
      d->token_painter->SizeHint(option, qvariant_cast<TokenRange>(val));

  QStyleOptionViewItem opt(option);
  initStyleOption(&opt, index);
  QStyle *style = opt.widget ? opt.widget->style() : qApp->style();

  QFontMetricsF font_metrics(option.font);
  QMargins margins(0, 0, static_cast<int>(font_metrics.horizontalAdvance("x")),
                   0);

  return style
      ->sizeFromContents(QStyle::ContentsType::CT_ItemViewItem, &opt,
                         contents_size, opt.widget)
      .grownBy(margins);
}

void ReferenceExplorerItemDelegate::OnThemeChange(
    const QPalette &, const CodeViewTheme &code_view_theme) {
  SetTheme(code_view_theme);
}

bool ReferenceExplorerItemDelegate::editorEvent(QEvent *, QAbstractItemModel *,
                                                const QStyleOptionViewItem &,
                                                const QModelIndex &) {
  return false;
}

}  // namespace mx::gui
