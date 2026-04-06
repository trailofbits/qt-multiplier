// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Widgets/SpreadsheetView.h>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>

#include <multiplier/GUI/Widgets/SpreadsheetModel.h>

namespace mx::gui {

SpreadsheetView::SpreadsheetView(QWidget *parent)
    : QTableView(parent) {

  // Sorting.
  setSortingEnabled(true);

  // Movable headers.
  horizontalHeader()->setSectionsMovable(true);
  verticalHeader()->setSectionsMovable(true);

  // Extended selection with cell-level granularity.
  setSelectionMode(QAbstractItemView::ExtendedSelection);
  setSelectionBehavior(QAbstractItemView::SelectItems);

  // Smooth scrolling.
  setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

  // Word wrap off for performance.
  setWordWrap(false);
  setTextElideMode(Qt::ElideRight);

  // Context menu.
  setContextMenuPolicy(Qt::CustomContextMenu);
  connect(this, &QWidget::customContextMenuRequested,
          this, &SpreadsheetView::OnContextMenu);
}

SpreadsheetView::~SpreadsheetView(void) {}

void SpreadsheetView::copy_selection(void) {
  auto indexes = selectionModel()->selectedIndexes();
  if (indexes.isEmpty()) {
    return;
  }

  // Sort by row then column.
  std::sort(indexes.begin(), indexes.end(),
            [](const QModelIndex &a, const QModelIndex &b) {
              if (a.row() != b.row()) return a.row() < b.row();
              return a.column() < b.column();
            });

  QString text;
  int prev_row = indexes.first().row();

  for (const auto &idx : indexes) {
    if (idx.row() != prev_row) {
      text += QLatin1Char('\n');
      prev_row = idx.row();
    } else if (&idx != &indexes.first()) {
      text += QLatin1Char('\t');
    }
    text += idx.data(Qt::DisplayRole).toString();
  }

  QApplication::clipboard()->setText(text);
}

void SpreadsheetView::paste_at_selection(void) {
  auto *clip = QApplication::clipboard();
  if (!clip) {
    return;
  }

  QString text = clip->text();
  if (text.isEmpty()) {
    return;
  }

  QModelIndex current = currentIndex();
  if (!current.isValid() || !model()) {
    return;
  }

  QStringList rows = text.split(QLatin1Char('\n'));
  int start_row = current.row();
  int start_col = current.column();

  for (int r = 0; r < rows.size(); ++r) {
    QStringList cells = rows[r].split(QLatin1Char('\t'));
    for (int c = 0; c < cells.size(); ++c) {
      QModelIndex idx = model()->index(start_row + r, start_col + c);
      if (idx.isValid()) {
        model()->setData(idx, cells[c], Qt::EditRole);
      }
    }
  }
}

void SpreadsheetView::cut_selection(void) {
  copy_selection();
  delete_selection();
}

void SpreadsheetView::delete_selection(void) {
  auto indexes = selectionModel()->selectedIndexes();
  for (const auto &idx : indexes) {
    if (idx.flags() & Qt::ItemIsEditable) {
      model()->setData(idx, QString(), Qt::EditRole);
    }
  }
}

void SpreadsheetView::copy_as_markdown(void) {
  // TODO(Phase 4): Emit markdown-formatted table to clipboard.
  copy_selection();
}

void SpreadsheetView::copy_as_html(void) {
  // TODO(Phase 4): Emit HTML-formatted table to clipboard.
  copy_selection();
}

void SpreadsheetView::keyPressEvent(QKeyEvent *event) {
  if (event->matches(QKeySequence::Copy)) {
    copy_selection();
    return;
  }
  if (event->matches(QKeySequence::Paste)) {
    paste_at_selection();
    return;
  }
  if (event->matches(QKeySequence::Cut)) {
    cut_selection();
    return;
  }
  if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
    delete_selection();
    return;
  }
  if (event->matches(QKeySequence::Undo)) {
    if (auto *sm = qobject_cast<SpreadsheetModel *>(model())) {
      sm->undoStack()->undo();
    }
    return;
  }
  if (event->matches(QKeySequence::Redo)) {
    if (auto *sm = qobject_cast<SpreadsheetModel *>(model())) {
      sm->undoStack()->redo();
    }
    return;
  }
  // Ctrl+Shift+C -> copy as markdown.
  if (event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier) &&
      event->key() == Qt::Key_C) {
    copy_as_markdown();
    return;
  }

  QTableView::keyPressEvent(event);
}

void SpreadsheetView::OnContextMenu(const QPoint &pos) {
  QMenu menu(this);

  // Undo/redo.
  if (auto *sm = qobject_cast<SpreadsheetModel *>(model())) {
    auto *undo = sm->undoStack()->createUndoAction(&menu, tr("Undo"));
    undo->setShortcut(QKeySequence::Undo);
    menu.addAction(undo);

    auto *redo = sm->undoStack()->createRedoAction(&menu, tr("Redo"));
    redo->setShortcut(QKeySequence::Redo);
    menu.addAction(redo);

    menu.addSeparator();
  }

  // Clipboard section.
  menu.addAction(tr("Copy"), QKeySequence::Copy,
                 this, &SpreadsheetView::copy_selection);
  menu.addAction(tr("Paste"), QKeySequence::Paste,
                 this, &SpreadsheetView::paste_at_selection);
  menu.addAction(tr("Cut"), QKeySequence::Cut,
                 this, &SpreadsheetView::cut_selection);
  menu.addAction(tr("Delete"), QKeySequence::Delete,
                 this, &SpreadsheetView::delete_selection);
  menu.addSeparator();
  menu.addAction(tr("Copy as Markdown"), this,
                 &SpreadsheetView::copy_as_markdown);
  menu.addAction(tr("Copy as HTML"), this,
                 &SpreadsheetView::copy_as_html);

  menu.addSeparator();

  // Row / column operations.
  QModelIndex idx = indexAt(pos);
  if (idx.isValid() && model()) {
    int row = idx.row();
    int col = idx.column();

    menu.addAction(tr("Insert Row Above"), this, [this, row]() {
      model()->insertRow(row);
    });
    menu.addAction(tr("Insert Row Below"), this, [this, row]() {
      model()->insertRow(row + 1);
    });
    menu.addAction(tr("Remove Row"), this, [this, row]() {
      model()->removeRow(row);
    });
    menu.addSeparator();
    menu.addAction(tr("Insert Column Left"), this, [this, col]() {
      model()->insertColumn(col);
    });
    menu.addAction(tr("Insert Column Right"), this, [this, col]() {
      model()->insertColumn(col + 1);
    });
    menu.addAction(tr("Remove Column"), this, [this, col]() {
      model()->removeColumn(col);
    });
  }

  menu.exec(viewport()->mapToGlobal(pos));
}

}  // namespace mx::gui
