/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "InformationExplorerItemDelegate.h"
#include "InformationExplorerModel.h"
#include "Utils.h"

#include <multiplier/GUI/TokenPainter.h>
#include <multiplier/GUI/ThemeManager.h>

#include <QFontMetrics>
#include <QPainter>

namespace mx::gui {

struct InformationExplorerItemDelegate::PrivateData final {
  CodeViewTheme code_view_theme;
  std::unique_ptr<TokenPainter> token_painter;
};

InformationExplorerItemDelegate::InformationExplorerItemDelegate(
    QObject *parent)
    : QStyledItemDelegate(parent),
      d(new PrivateData) {

  d->code_view_theme = ThemeManager::Get().GetCodeViewTheme();

  TokenPainterConfiguration painter_config(d->code_view_theme);
  d->token_painter = std::make_unique<TokenPainter>(painter_config);
}

InformationExplorerItemDelegate::~InformationExplorerItemDelegate() {}

QSize InformationExplorerItemDelegate::sizeHint(
    const QStyleOptionViewItem &option, const QModelIndex &index) const {

  if (!ShouldPaintAsTokens(index)) {
    return QStyledItemDelegate::sizeHint(option, index);
  }

  auto token_range_var = index.data(InformationExplorerModel::TokenRangeRole);
  const auto &token_range = qvariant_cast<TokenRange>(token_range_var);

  return d->token_painter->SizeHint(option, token_range);
}

void InformationExplorerItemDelegate::paint(QPainter *painter,
                                            const QStyleOptionViewItem &option,
                                            const QModelIndex &index) const {

  if (!ShouldPaintAsTokens(index)) {
    QStyledItemDelegate::paint(painter, option, index);
    return;
  }

  auto token_range_var = index.data(InformationExplorerModel::TokenRangeRole);
  d->token_painter->Paint(painter, option,
                          qvariant_cast<TokenRange>(token_range_var));
}

bool InformationExplorerItemDelegate::editorEvent(QEvent *,
                                                  QAbstractItemModel *,
                                                  const QStyleOptionViewItem &,
                                                  const QModelIndex &) {
  return false;
}

}  // namespace mx::gui
