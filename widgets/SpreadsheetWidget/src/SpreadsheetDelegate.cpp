// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Widgets/SpreadsheetDelegate.h>

#include <QApplication>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionViewItem>

#include <multiplier/Frontend/Token.h>
#include <multiplier/GUI/Widgets/SpreadsheetModel.h>

Q_DECLARE_METATYPE(mx::Token)
Q_DECLARE_METATYPE(mx::TokenRange)

namespace mx::gui {

SpreadsheetDelegate::SpreadsheetDelegate(QObject *parent)
    : QStyledItemDelegate(parent) {}

SpreadsheetDelegate::~SpreadsheetDelegate(void) {}

void SpreadsheetDelegate::paint(QPainter *painter,
                                const QStyleOptionViewItem &option,
                                const QModelIndex &index) const {
  QVariant raw = index.data(SpreadsheetRoles::RawValueRole);

  // Token cells: render as plain text with a subtle colour hint.
  // TODO(Phase 4): Wrap ThemedItemDelegate for proper token painting,
  // following the ColumnTintDelegate pattern from CodeSearchExplorer.
  if (raw.canConvert<Token>() || raw.canConvert<TokenRange>()) {
    // Draw selection / focus background via the default implementation's
    // background handling.
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    // Let the style draw the background (handles selection highlighting).
    auto *style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter,
                         opt.widget);

    // Draw the display text.
    QString text = index.data(Qt::DisplayRole).toString();
    QColor fg = opt.palette.color(
        (opt.state & QStyle::State_Selected) ? QPalette::HighlightedText
                                             : QPalette::Text);
    painter->save();
    painter->setPen(fg);
    painter->setFont(opt.font);
    QRect text_rect = style->subElementRect(
        QStyle::SE_ItemViewItemText, &opt, opt.widget);
    painter->drawText(text_rect, Qt::AlignLeft | Qt::AlignVCenter, text);
    painter->restore();
    return;
  }

  // FormulaCell: render the cached result (or error) text. Errors get a
  // red foreground.
  if (raw.canConvert<FormulaCell>()) {
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    auto fc = raw.value<FormulaCell>();
    if (!fc.error_message.isEmpty()) {
      opt.palette.setColor(QPalette::Text, QColor(Qt::red));
      opt.palette.setColor(QPalette::HighlightedText, QColor(Qt::red));
    }

    QStyledItemDelegate::paint(painter, opt, index);
    return;
  }

  // Bool and QString: fall through to the default delegate.
  QStyledItemDelegate::paint(painter, option, index);
}

QWidget *SpreadsheetDelegate::createEditor(
    QWidget *parent, const QStyleOptionViewItem &option,
    const QModelIndex &index) const {

  QVariant raw = index.data(SpreadsheetRoles::RawValueRole);

  // Token and bool cells are not text-editable.
  if (raw.canConvert<Token>() || raw.canConvert<TokenRange>()) {
    return nullptr;
  }
  if (raw.userType() == QMetaType::Bool) {
    return nullptr;
  }

  return QStyledItemDelegate::createEditor(parent, option, index);
}

}  // namespace mx::gui
