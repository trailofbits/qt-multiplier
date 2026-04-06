// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Widgets/SpreadsheetView.h>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QColorDialog>
#include <QDataStream>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>

#include <multiplier/Index.h>

#include <iostream>

Q_DECLARE_METATYPE(mx::TokenRange)

#include <multiplier/GUI/Widgets/SimpleTextInputDialog.h>
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

  // Visual structure.
  setShowGrid(true);
  setAlternatingRowColors(true);

  // Word wrap on for multiline cells.
  setWordWrap(true);
  setTextElideMode(Qt::ElideNone);
  verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

  // Context menu on row headers for insert/delete/move/color.
  verticalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(verticalHeader(), &QWidget::customContextMenuRequested,
          this, [this] (const QPoint &pos) {
    int row = verticalHeader()->logicalIndexAt(pos);
    if (row < 0) return;

    QMenu menu(this);
    menu.addAction(tr("Insert Row Above"), this, [this, row] () {
      model()->insertRow(row);
    });
    menu.addAction(tr("Insert Row Below"), this, [this, row] () {
      model()->insertRow(row + 1);
    });
    menu.addAction(tr("Remove Row"), this, [this, row] () {
      model()->removeRow(row);
    });
    menu.addSeparator();
    menu.addAction(tr("Set Row Color..."), this, [this, row] () {
      auto *sm = qobject_cast<SpreadsheetModel *>(model());
      if (!sm) return;
      QColor initial = sm->RowColor(row);
      QColor color = QColorDialog::getColor(
          initial.isValid() ? initial : Qt::white, this,
          tr("Row Color"));
      if (color.isValid()) {
        sm->SetRowColor(row, color);
      }
    });
    menu.addAction(tr("Clear Row Color"), this, [this, row] () {
      if (auto *sm = qobject_cast<SpreadsheetModel *>(model())) {
        sm->ClearRowColor(row);
      }
    });
    menu.exec(verticalHeader()->mapToGlobal(pos));
  });

  // Context menu on cells.
  setContextMenuPolicy(Qt::CustomContextMenu);
  connect(this, &QWidget::customContextMenuRequested,
          this, &SpreadsheetView::OnContextMenu);

  // Context menu on column headers for rename/delete/insert.
  horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(horizontalHeader(), &QWidget::customContextMenuRequested,
          this, [this] (const QPoint &pos) {
    int col = horizontalHeader()->logicalIndexAt(pos);
    if (col < 0) return;

    QMenu menu(this);
    menu.addAction(tr("Rename Column..."), this, [this, col] () {
      auto current = model()->headerData(col, Qt::Horizontal).toString();
      SimpleTextInputDialog dialog(tr("Enter the new column name"),
                                   current, this);
      dialog.setWindowTitle(tr("Rename Column"));
      if (dialog.exec() == QDialog::Accepted) {
        auto opt_name = dialog.TextInput();
        if (opt_name.has_value() && !opt_name->isEmpty()) {
          model()->setHeaderData(col, Qt::Horizontal, opt_name.value());
        }
      }
    });
    menu.addSeparator();
    menu.addAction(tr("Insert Column Left"), this, [this, col] () {
      model()->insertColumn(col);
    });
    menu.addAction(tr("Insert Column Right"), this, [this, col] () {
      model()->insertColumn(col + 1);
    });
    menu.addAction(tr("Remove Column"), this, [this, col] () {
      model()->removeColumn(col);
    });
    menu.addSeparator();
    menu.addAction(tr("Set Column Color..."), this, [this, col] () {
      auto *sm = qobject_cast<SpreadsheetModel *>(model());
      if (!sm) return;
      QColor initial = sm->ColumnColor(col);
      QColor color = QColorDialog::getColor(
          initial.isValid() ? initial : Qt::white, this,
          tr("Column Color"));
      if (color.isValid()) {
        sm->SetColumnColor(col, color);
      }
    });
    menu.addAction(tr("Clear Column Color"), this, [this, col] () {
      if (auto *sm = qobject_cast<SpreadsheetModel *>(model())) {
        sm->ClearColumnColor(col);
      }
    });
    menu.exec(horizontalHeader()->mapToGlobal(pos));
  });
}

SpreadsheetView::~SpreadsheetView(void) {}

void SpreadsheetView::ApplyThemeColors(const QColor &gutter_bg,
                                       const QColor &gutter_fg,
                                       const QColor &grid_color) {
  auto header_style = QStringLiteral(
      "QHeaderView::section {"
      "  background-color: %1;"
      "  color: %2;"
      "  border: 1px solid %3;"
      "  padding: 2px 4px;"
      "}")
      .arg(gutter_bg.name(), gutter_fg.name(), grid_color.name());

  horizontalHeader()->setStyleSheet(header_style);
  verticalHeader()->setStyleSheet(header_style);

  setGridStyle(Qt::SolidLine);

  // Set grid color via stylesheet on the table itself.
  setStyleSheet(QStringLiteral(
      "QTableView { gridline-color: %1; }")
      .arg(grid_color.name()));
}

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
  if (!clip || !clip->mimeData()) {
    return;
  }

  QModelIndex current = currentIndex();
  if (!current.isValid() || !model()) {
    return;
  }

  const QMimeData *mime = clip->mimeData();

  std::cerr << "PASTE: formats=";
  for (const auto &f : mime->formats()) {
    std::cerr << f.toStdString() << " ";
  }
  std::cerr << std::endl;

  // Check for token range data from the code explorer.
  if (mime->hasFormat(
          QStringLiteral("application/x-qtmultiplier-tokens"))) {
    auto data = mime->data(
        QStringLiteral("application/x-qtmultiplier-tokens"));
    QDataStream stream(&data, QIODevice::ReadOnly);

    quint32 count = 0;
    stream >> count;
    std::cerr << "PASTE: token count=" << count << std::endl;

    // Collect tokens into one cell as a TokenRange.
    std::vector<CustomToken> tokens;
    for (quint32 i = 0; i < count; ++i) {
      quint64 entity_id = 0;
      quint32 kind = 0;
      quint32 category = 0;
      QString display_text;
      stream >> entity_id >> kind >> category >> display_text;

      UserToken ut;
      ut.data = display_text.toStdString();
      ut.kind = static_cast<TokenKind>(kind);
      ut.category = static_cast<TokenCategory>(category);
      tokens.emplace_back(std::move(ut));
    }

    if (!tokens.empty()) {
      auto range = TokenRange::create(std::move(tokens));
      std::cerr << "PASTE: created TokenRange size=" << range.size()
                << " empty=" << range.empty() << std::endl;
      auto var = QVariant::fromValue(range);
      std::cerr << "PASTE: variant type=" << var.typeName()
                << " canConvertTR=" << var.canConvert<TokenRange>()
                << std::endl;
      if (auto *sm = qobject_cast<SpreadsheetModel *>(model())) {
        sm->set_cell_value(current.row(), current.column(), var);
        std::cerr << "PASTE: set_cell_value called" << std::endl;
      }
      return;
    }
  }

  // Fall back to plain text paste.
  QString text = mime->text();
  if (text.isEmpty()) {
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

  // "Paste as Text" — forces plain text paste even if tokens are available.
  {
    auto *clip = QApplication::clipboard();
    if (clip && clip->mimeData() &&
        clip->mimeData()->hasFormat(
            QStringLiteral("application/x-qtmultiplier-tokens"))) {
      menu.addAction(tr("Paste as Text"), this, [this] () {
        auto *clip = QApplication::clipboard();
        if (!clip) return;
        QString text = clip->text();
        if (text.isEmpty()) return;
        QModelIndex current = currentIndex();
        if (!current.isValid() || !model()) return;
        model()->setData(current, text, Qt::EditRole);
      });
    }
  }
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
