/*
  Copyright (c) 2024-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QStyledItemDelegate>

namespace mx::gui {

// Item delegate for the spreadsheet view. Renders Token / TokenRange cells
// using theme colours (plain text for Phase 1; proper ThemedItemDelegate
// wrapping will be added in Phase 4). FormulaCell values display their
// cached result. Bool cells are rendered as check-boxes via the default
// delegate. Other types fall through to QStyledItemDelegate::paint.
class SpreadsheetDelegate Q_DECL_FINAL : public QStyledItemDelegate {
  Q_OBJECT

 public:
  explicit SpreadsheetDelegate(QObject *parent = nullptr);
  virtual ~SpreadsheetDelegate(void);

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const Q_DECL_FINAL;

  QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                        const QModelIndex &index) const Q_DECL_FINAL;
};

}  // namespace mx::gui
