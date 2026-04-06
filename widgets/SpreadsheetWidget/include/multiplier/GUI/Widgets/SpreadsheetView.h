/*
  Copyright (c) 2024-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QTableView>

QT_BEGIN_NAMESPACE
class QKeyEvent;
class QMenu;
QT_END_NAMESPACE

namespace mx::gui {

// A QTableView subclass providing spreadsheet-like interaction: keyboard
// shortcuts for clipboard operations, context menus for row/column
// manipulation, and extended cell-level selection.
class SpreadsheetView Q_DECL_FINAL : public QTableView {
  Q_OBJECT

 public:
  explicit SpreadsheetView(QWidget *parent = nullptr);
  virtual ~SpreadsheetView(void);

  // Update header/grid colors from theme.
  void ApplyThemeColors(const QColor &gutter_bg, const QColor &gutter_fg,
                        const QColor &grid_color);

  // Clipboard operations.
  void copy_selection(void);
  void paste_at_selection(void);
  void cut_selection(void);
  void delete_selection(void);

  // Fancy clipboard formats (Phase 4 stubs).
  void copy_as_markdown(void);
  void copy_as_html(void);

 protected:
  void keyPressEvent(QKeyEvent *event) Q_DECL_FINAL;
  bool eventFilter(QObject *object, QEvent *event) Q_DECL_FINAL;

 private slots:
  void OnContextMenu(const QPoint &pos);
};

}  // namespace mx::gui
