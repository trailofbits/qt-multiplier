// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Widgets/SpreadsheetView.h>

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QColorDialog>
#include <QDataStream>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QMimeData>
#include <QSortFilterProxyModel>

#include <multiplier/Index.h>

Q_DECLARE_METATYPE(mx::TokenRange)

#include <multiplier/Frontend/TokenKind.h>
#include <multiplier/GUI/Interfaces/ITheme.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/ThemeManager.h>
#include <multiplier/GUI/Widgets/SimpleTextInputDialog.h>
#include <multiplier/GUI/Widgets/SpreadsheetModel.h>

Q_DECLARE_METATYPE(mx::Token)

namespace mx::gui {

SpreadsheetView::SpreadsheetView(QWidget *parent)
    : QTableView(parent) {

  // Sorting via context menu only (stable sort in model).
  setSortingEnabled(false);
  horizontalHeader()->setSortIndicatorShown(true);

  // Cmd/Ctrl+click on a column header selects the column without sorting.
  connect(horizontalHeader(), &QHeaderView::sectionClicked,
          this, [this] (int col) {
    if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
      selectColumn(col);
    }
  });

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
  verticalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  verticalHeader()->setDefaultSectionSize(
      fontMetrics().height() + 8);  // Sensible default.
  resizeRowsToContents();  // Initial fit.

  // Context menu on the corner button (select-all).
  if (auto *corner = findChild<QAbstractButton *>()) {
    corner->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(corner, &QWidget::customContextMenuRequested,
            this, [this, corner] (const QPoint &pos) {
      QMenu menu(this);
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
      menu.addAction(tr("Copy as CSV"), this,
                     &SpreadsheetView::copy_as_csv);
      menu.addAction(tr("Copy as TSV"), this,
                     &SpreadsheetView::copy_as_tsv);
      menu.exec(corner->mapToGlobal(pos));
    });
  }

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
    menu.addSeparator();

    auto *clickable_action = menu.addAction(tr("Clickable Tokens"));
    clickable_action->setCheckable(true);
    clickable_action->setChecked(clickable_columns_.contains(col));
    connect(clickable_action, &QAction::toggled, this,
            [this, col] (bool checked) {
      SetColumnClickable(col, checked);
    });

    menu.addSeparator();
    menu.addAction(tr("Sort Ascending"), this, [this, col] () {
      sortByColumn(col, Qt::AscendingOrder);
      horizontalHeader()->setSortIndicator(col, Qt::AscendingOrder);
    });
    menu.addAction(tr("Sort Descending"), this, [this, col] () {
      sortByColumn(col, Qt::DescendingOrder);
      horizontalHeader()->setSortIndicator(col, Qt::DescendingOrder);
    });
    menu.exec(horizontalHeader()->mapToGlobal(pos));
  });
}

SpreadsheetView::~SpreadsheetView(void) {}

void SpreadsheetView::SetColumnClickable(int col, bool clickable) {
  if (clickable) {
    clickable_columns_.insert(col);
  } else {
    clickable_columns_.remove(col);
  }
}

void SpreadsheetView::mousePressEvent(QMouseEvent *event) {
  auto idx = indexAt(event->pos());
  if (idx.isValid()) {
    QVariant raw = idx.data(SpreadsheetRoles::RawValueRole);

    // Document cells: open viewer on click, but also allow normal selection.
    if (raw.canConvert<DocumentCell>()) {
      emit DocumentCellClicked(idx);
    }

    // Clickable-tokens columns: navigate on click, select only the cell.
    if (clickable_columns_.contains(idx.column())) {
      selectionModel()->select(idx, QItemSelectionModel::ClearAndSelect);
      setCurrentIndex(idx);
      emit TokenClicked(idx);
      event->accept();
      return;
    }
  }
  QTableView::mousePressEvent(event);
}

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

  // Plain text (tab-separated).
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

  // Rich format: serialize raw cell values as JSON grid so cross-sheet
  // paste preserves tokens, booleans, colors, etc.
  int min_row = indexes.first().row(), min_col = indexes.first().column();
  int max_row = min_row, max_col = min_col;
  for (const auto &idx : indexes) {
    min_row = std::min(min_row, idx.row());
    max_row = std::max(max_row, idx.row());
    min_col = std::min(min_col, idx.column());
    max_col = std::max(max_col, idx.column());
  }

  // Build a JSON array of rows, each row an array of JSON cell strings.
  QByteArray rich;
  QDataStream stream(&rich, QIODevice::WriteOnly);
  stream << static_cast<qint32>(max_row - min_row + 1);
  stream << static_cast<qint32>(max_col - min_col + 1);

  // Build a map for quick lookup.
  QMap<QPair<int,int>, QModelIndex> grid;
  for (const auto &idx : indexes) {
    grid[{idx.row(), idx.column()}] = idx;
  }

  for (int r = min_row; r <= max_row; ++r) {
    for (int c = min_col; c <= max_col; ++c) {
      auto it = grid.find({r, c});
      if (it != grid.end()) {
        QVariant raw = it.value().data(SpreadsheetRoles::RawValueRole);
        if (raw.isValid()) {
          stream << SpreadsheetModel::value_to_json(raw);
        } else {
          stream << QString();
        }
      } else {
        stream << QString();
      }
    }
  }

  auto *mime = new QMimeData;
  mime->setText(text);
  mime->setData(QStringLiteral("application/x-qtmultiplier-sheet-cells"),
                rich);
  QApplication::clipboard()->setMimeData(mime);
}

void SpreadsheetView::paste_at_selection(void) {
  auto *clip = QApplication::clipboard();
  if (!clip || !clip->mimeData()) {
    return;
  }

  if (!model()) return;

  // Use the top-left cell of the selection as the paste origin.
  QModelIndex current;
  auto sel = selectionModel()->selectedIndexes();
  if (!sel.isEmpty()) {
    int min_row = sel.first().row(), min_col = sel.first().column();
    for (const auto &idx : sel) {
      min_row = std::min(min_row, idx.row());
      min_col = std::min(min_col, idx.column());
    }
    current = model()->index(min_row, min_col);
  } else {
    current = currentIndex();
  }
  if (!current.isValid()) return;

  const QMimeData *mime = clip->mimeData();

  // Resolve the source SpreadsheetModel through any proxy.
  SpreadsheetModel *sm = nullptr;
  auto *m = model();
  if (auto *proxy = qobject_cast<QSortFilterProxyModel *>(m)) {
    sm = qobject_cast<SpreadsheetModel *>(proxy->sourceModel());
  } else {
    sm = qobject_cast<SpreadsheetModel *>(m);
  }

  int start_row = current.row();
  int start_col = current.column();

  // Check for document reference paste.
  if (mime->hasFormat(
          QStringLiteral("application/x-qtmultiplier-document"))) {
    auto data = mime->data(
        QStringLiteral("application/x-qtmultiplier-document"));
    QDataStream stream(&data, QIODevice::ReadOnly);
    qint32 doc_id = -1;
    stream >> doc_id;
    if (doc_id >= 0 && sm) {
      DocumentCell dc;
      dc.doc_id = doc_id;
      // Load the title from the DB for cell display.
      if (config_manager_) {
        dc.title = config_manager_->LoadDocumentTitle(doc_id);
      }
      sm->set_cell_value(start_row, start_col,
                         QVariant::fromValue(dc));
    }
    return;
  }

  // Check for location reference paste (from CodeWidget "Copy Location").
  if (mime->hasFormat(
          QStringLiteral("application/x-multiplier-location"))) {
    auto data = mime->data(
        QStringLiteral("application/x-multiplier-location"));
    auto doc = QJsonDocument::fromJson(data);
    if (doc.isObject() && sm) {
      auto obj = doc.object();
      LocationCell lc;
      lc.entity_id = static_cast<uint64_t>(
          obj[QStringLiteral("e")].toDouble());
      lc.file_path = obj[QStringLiteral("p")].toString();
      lc.line = static_cast<unsigned>(
          obj[QStringLiteral("l")].toInt());
      lc.column = static_cast<unsigned>(
          obj[QStringLiteral("c")].toInt());
      lc.opaque_data = QByteArray::fromBase64(
          obj[QStringLiteral("o")].toString().toLatin1());
      sm->set_cell_value(start_row, start_col,
                         QVariant::fromValue(lc));
    }
    return;
  }

  // Check for sheet cell grid data (cross-sheet copy).
  if (mime->hasFormat(
          QStringLiteral("application/x-qtmultiplier-sheet-cells"))) {
    auto data = mime->data(
        QStringLiteral("application/x-qtmultiplier-sheet-cells"));
    QDataStream stream(&data, QIODevice::ReadOnly);

    qint32 num_rows = 0, num_cols = 0;
    stream >> num_rows >> num_cols;

    if (sm) {
      sm->undoStack()->beginMacro(tr("Paste"));

      // Expand to fit.
      while (sm->rowCount() < start_row + num_rows) {
        sm->insertRow(sm->rowCount());
      }
      while (sm->columnCount() < start_col + num_cols) {
        sm->insertColumn(sm->columnCount());
      }

      const mx::Index *index = config_manager_
          ? &config_manager_->Index() : nullptr;

      for (int r = 0; r < num_rows; ++r) {
        for (int c = 0; c < num_cols; ++c) {
          QString json;
          stream >> json;
          if (json.isEmpty()) {
            sm->set_cell_value(start_row + r, start_col + c, QVariant());
          } else {
            sm->set_cell_value(start_row + r, start_col + c,
                SpreadsheetModel::value_from_json(json, index));
          }
        }
      }

      sm->undoStack()->endMacro();
    }
    return;
  }

  // Check for token range data from the code explorer.
  if (mime->hasFormat(
          QStringLiteral("application/x-qtmultiplier-tokens"))) {
    auto data = mime->data(
        QStringLiteral("application/x-qtmultiplier-tokens"));
    QDataStream stream(&data, QIODevice::ReadOnly);

    quint32 count = 0;
    stream >> count;

    // Collect tokens into one cell as a TokenRange.
    std::vector<CustomToken> tokens;
    for (quint32 i = 0; i < count; ++i) {
      quint64 token_id = 0;
      quint32 kind = 0;
      quint32 category = 0;
      QString display_text;
      quint64 related_eid = 0;
      stream >> token_id >> kind >> category >> display_text;

      // Read the related entity ID if present (newer format).
      if (!stream.atEnd()) {
        stream >> related_eid;
      }

      UserToken ut;
      ut.data = display_text.toStdString();
      ut.kind = static_cast<TokenKind>(kind);
      ut.category = static_cast<TokenCategory>(category);

      // Resolve the related entity from the index so that
      // highlight colors carry through to sheet cells.
      if (related_eid != kInvalidEntityId && config_manager_) {
        ut.related_entity =
            config_manager_->Index().entity(EntityId(related_eid));
      }

      tokens.emplace_back(std::move(ut));
    }

    // Strip leading and trailing whitespace tokens.
    while (!tokens.empty()) {
      auto *ut = std::get_if<UserToken>(&tokens.front());
      if (ut && ut->kind == TokenKind::WHITESPACE) {
        tokens.erase(tokens.begin());
      } else {
        break;
      }
    }
    while (!tokens.empty()) {
      auto *ut = std::get_if<UserToken>(&tokens.back());
      if (ut && ut->kind == TokenKind::WHITESPACE) {
        tokens.pop_back();
      } else {
        break;
      }
    }

    if (!tokens.empty() && sm) {
      auto range = TokenRange::create(std::move(tokens));
      sm->set_cell_value(start_row, start_col,
                         QVariant::fromValue(range));
      return;
    }
  }

  // Fall back to plain text paste.
  QString text = mime->text();
  if (text.isEmpty()) {
    return;
  }

  QStringList rows = text.split(QLatin1Char('\n'));

  if (sm) {
    sm->undoStack()->beginMacro(tr("Paste"));

    // Expand to fit.
    int max_cols = 0;
    for (const auto &row : rows) {
      max_cols = std::max(max_cols,
                          static_cast<int>(row.split(QLatin1Char('\t')).size()));
    }
    while (sm->rowCount() < start_row + rows.size()) {
      sm->insertRow(sm->rowCount());
    }
    while (sm->columnCount() < start_col + max_cols) {
      sm->insertColumn(sm->columnCount());
    }

    for (int r = 0; r < rows.size(); ++r) {
      QStringList cells = rows[r].split(QLatin1Char('\t'));
      for (int c = 0; c < cells.size(); ++c) {
        sm->set_cell_value(start_row + r, start_col + c,
                           QVariant(cells[c]));
      }
    }

    sm->undoStack()->endMacro();
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
  auto indexes = selectionModel()->selectedIndexes();
  if (indexes.isEmpty()) return;

  std::sort(indexes.begin(), indexes.end(),
            [](const QModelIndex &a, const QModelIndex &b) {
              if (a.row() != b.row()) return a.row() < b.row();
              return a.column() < b.column();
            });

  // Determine column range.
  int min_col = indexes.first().column();
  int max_col = min_col;
  for (const auto &idx : indexes) {
    min_col = std::min(min_col, idx.column());
    max_col = std::max(max_col, idx.column());
  }

  // Build a row-major map of cell texts.
  QMap<int, QMap<int, QString>> grid;
  for (const auto &idx : indexes) {
    QVariant raw = idx.data(SpreadsheetRoles::RawValueRole);
    QString text;
    if (raw.userType() == QMetaType::Bool) {
      text = raw.toBool() ? QStringLiteral("[x]") : QStringLiteral("[ ]");
    } else if (raw.canConvert<TokenRange>() || raw.canConvert<Token>()) {
      // Wrap token text in backticks.
      text = QLatin1Char('`') + idx.data(Qt::DisplayRole).toString()
             + QLatin1Char('`');
    } else {
      text = idx.data(Qt::DisplayRole).toString();
    }
    // Escape pipe characters.
    text.replace(QLatin1Char('|'), QStringLiteral("\\|"));
    grid[idx.row()][idx.column()] = text;
  }

  // Header row.
  QString md;
  QString sep;
  for (int c = min_col; c <= max_col; ++c) {
    QString hdr = model()->headerData(c, Qt::Horizontal).toString();
    hdr.replace(QLatin1Char('|'), QStringLiteral("\\|"));
    md += QStringLiteral("| ") + hdr + QLatin1Char(' ');
    sep += QStringLiteral("| --- ");
  }
  md += QStringLiteral("|\n");
  sep += QStringLiteral("|\n");
  md += sep;

  // Data rows.
  for (auto row_it = grid.constBegin(); row_it != grid.constEnd(); ++row_it) {
    for (int c = min_col; c <= max_col; ++c) {
      md += QStringLiteral("| ") + row_it.value().value(c) + QLatin1Char(' ');
    }
    md += QStringLiteral("|\n");
  }

  QApplication::clipboard()->setText(md);
}

void SpreadsheetView::copy_as_html(void) {
  auto indexes = selectionModel()->selectedIndexes();
  if (indexes.isEmpty()) return;

  std::sort(indexes.begin(), indexes.end(),
            [](const QModelIndex &a, const QModelIndex &b) {
              if (a.row() != b.row()) return a.row() < b.row();
              return a.column() < b.column();
            });

  int min_col = indexes.first().column();
  int max_col = min_col;
  for (const auto &idx : indexes) {
    min_col = std::min(min_col, idx.column());
    max_col = std::max(max_col, idx.column());
  }

  IThemePtr theme;
  if (config_manager_) {
    theme = config_manager_->ThemeManager().Theme();
  }

  auto cell_html = [&](const QModelIndex &idx) -> QString {
    QVariant raw = idx.data(SpreadsheetRoles::RawValueRole);

    if (raw.userType() == QMetaType::Bool) {
      return raw.toBool() ? QStringLiteral("&#9745;")
                          : QStringLiteral("&#9744;");
    }

    if (raw.canConvert<TokenRange>() && theme) {
      QString html;
      for (Token tok : raw.value<TokenRange>()) {
        if (tok.kind() == TokenKind::WHITESPACE) {
          auto td = tok.data();
          QString ws = QString::fromUtf8(td.data(),
                                         static_cast<qsizetype>(td.size()));
          ws.replace(QLatin1Char(' '), QStringLiteral("&nbsp;"));
          ws.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
          ws.replace(QLatin1Char('\t'), QStringLiteral("&nbsp;&nbsp;&nbsp;&nbsp;"));
          html += ws;
          continue;
        }
        auto cs = theme->TokenColorAndStyle(tok);
        if (!cs.foreground_color.isValid()) {
          cs.foreground_color = theme->DefaultForegroundColor();
        }
        auto td = tok.data();
        QString text = QString::fromUtf8(td.data(),
                                         static_cast<qsizetype>(td.size()))
                           .toHtmlEscaped();
        QString s = QStringLiteral("color:") + cs.foreground_color.name();
        if (cs.bold) s += QStringLiteral(";font-weight:bold");
        if (cs.italic) s += QStringLiteral(";font-style:italic");
        if (cs.background_color.isValid()) {
          s += QStringLiteral(";background:") + cs.background_color.name();
        }
        html += QStringLiteral("<span style=\"") + s
                + QStringLiteral("\">") + text
                + QStringLiteral("</span>");
      }
      return html;
    }

    if (raw.canConvert<Token>() && theme) {
      auto tok = raw.value<Token>();
      auto cs = theme->TokenColorAndStyle(tok);
      if (!cs.foreground_color.isValid()) {
        cs.foreground_color = theme->DefaultForegroundColor();
      }
      auto td = tok.data();
      QString text = QString::fromUtf8(td.data(),
                                       static_cast<qsizetype>(td.size()))
                         .toHtmlEscaped();
      QString s = QStringLiteral("color:") + cs.foreground_color.name();
      if (cs.bold) s += QStringLiteral(";font-weight:bold");
      if (cs.italic) s += QStringLiteral(";font-style:italic");
      return QStringLiteral("<span style=\"") + s
             + QStringLiteral("\">") + text + QStringLiteral("</span>");
    }

    return idx.data(Qt::DisplayRole).toString().toHtmlEscaped();
  };

  QMap<int, QMap<int, QString>> grid;
  for (const auto &idx : indexes) {
    grid[idx.row()][idx.column()] = cell_html(idx);
  }

  QString html = QStringLiteral(
      "<table border=1 cellpadding=4 cellspacing=0"
      " style=\"font-family:monospace;border-collapse:collapse\">\n<tr>");
  for (int c = min_col; c <= max_col; ++c) {
    html += QStringLiteral("<th>")
            + model()->headerData(c, Qt::Horizontal).toString().toHtmlEscaped()
            + QStringLiteral("</th>");
  }
  html += QStringLiteral("</tr>\n");

  for (auto row_it = grid.constBegin(); row_it != grid.constEnd(); ++row_it) {
    html += QStringLiteral("<tr>");
    for (int c = min_col; c <= max_col; ++c) {
      html += QStringLiteral("<td>") + row_it.value().value(c)
              + QStringLiteral("</td>");
    }
    html += QStringLiteral("</tr>\n");
  }
  html += QStringLiteral("</table>");

  auto *mime = new QMimeData;
  mime->setHtml(html);
  mime->setText(html);
  QApplication::clipboard()->setMimeData(mime);
}

void SpreadsheetView::copy_as_csv(void) {
  auto indexes = selectionModel()->selectedIndexes();
  if (indexes.isEmpty()) return;

  std::sort(indexes.begin(), indexes.end(),
            [](const QModelIndex &a, const QModelIndex &b) {
              if (a.row() != b.row()) return a.row() < b.row();
              return a.column() < b.column();
            });

  int min_col = indexes.first().column();
  int max_col = min_col;
  for (const auto &idx : indexes) {
    min_col = std::min(min_col, idx.column());
    max_col = std::max(max_col, idx.column());
  }

  // Build row-major grid of display text.
  QMap<int, QMap<int, QString>> grid;
  for (const auto &idx : indexes) {
    grid[idx.row()][idx.column()] = idx.data(Qt::DisplayRole).toString();
  }

  auto quote_csv = [](const QString &s) -> QString {
    if (s.contains(QLatin1Char(',')) || s.contains(QLatin1Char('"')) ||
        s.contains(QLatin1Char('\n'))) {
      QString escaped = s;
      escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
      return QLatin1Char('"') + escaped + QLatin1Char('"');
    }
    return s;
  };

  QString csv;

  // Header row.
  for (int c = min_col; c <= max_col; ++c) {
    if (c > min_col) csv += QLatin1Char(',');
    csv += quote_csv(model()->headerData(c, Qt::Horizontal).toString());
  }
  csv += QLatin1Char('\n');

  for (auto row_it = grid.constBegin(); row_it != grid.constEnd(); ++row_it) {
    for (int c = min_col; c <= max_col; ++c) {
      if (c > min_col) csv += QLatin1Char(',');
      csv += quote_csv(row_it.value().value(c));
    }
    csv += QLatin1Char('\n');
  }

  QApplication::clipboard()->setText(csv);
}

void SpreadsheetView::copy_as_tsv(void) {
  auto indexes = selectionModel()->selectedIndexes();
  if (indexes.isEmpty()) return;

  std::sort(indexes.begin(), indexes.end(),
            [](const QModelIndex &a, const QModelIndex &b) {
              if (a.row() != b.row()) return a.row() < b.row();
              return a.column() < b.column();
            });

  int min_col = indexes.first().column();
  int max_col = min_col;
  for (const auto &idx : indexes) {
    min_col = std::min(min_col, idx.column());
    max_col = std::max(max_col, idx.column());
  }

  QMap<int, QMap<int, QString>> grid;
  for (const auto &idx : indexes) {
    grid[idx.row()][idx.column()] = idx.data(Qt::DisplayRole).toString();
  }

  QString tsv;

  for (int c = min_col; c <= max_col; ++c) {
    if (c > min_col) tsv += QLatin1Char('\t');
    tsv += model()->headerData(c, Qt::Horizontal).toString();
  }
  tsv += QLatin1Char('\n');

  for (auto row_it = grid.constBegin(); row_it != grid.constEnd(); ++row_it) {
    for (int c = min_col; c <= max_col; ++c) {
      if (c > min_col) tsv += QLatin1Char('\t');
      tsv += row_it.value().value(c);
    }
    tsv += QLatin1Char('\n');
  }

  QApplication::clipboard()->setText(tsv);
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
        QString text = clip->text().trimmed();
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
  menu.addAction(tr("Copy as CSV"), this,
                 &SpreadsheetView::copy_as_csv);
  menu.addAction(tr("Copy as TSV"), this,
                 &SpreadsheetView::copy_as_tsv);

  menu.addSeparator();

  // Cell type operations.
  QModelIndex idx = indexAt(pos);
  if (idx.isValid() && model()) {
    QVariant raw = idx.data(SpreadsheetRoles::RawValueRole);
    if (!raw.canConvert<DocumentCell>()) {
      menu.addAction(tr("Insert Document"), this, [this, idx] () {
        DocumentCell dc;
        SpreadsheetModel *sm2 = nullptr;
        auto *m2 = model();
        if (auto *proxy = qobject_cast<QSortFilterProxyModel *>(m2)) {
          sm2 = qobject_cast<SpreadsheetModel *>(proxy->sourceModel());
        } else {
          sm2 = qobject_cast<SpreadsheetModel *>(m2);
        }
        if (sm2) {
          sm2->set_cell_value(idx.row(), idx.column(),
                              QVariant::fromValue(dc));
          emit DocumentCellClicked(
              model()->index(idx.row(), idx.column()));
        }
      });
    }

    // Clear Cell — works on all selected cells.
    menu.addAction(tr("Clear Cell"), this, [this] () {
      auto sel = selectionModel()->selectedIndexes();
      if (sel.isEmpty()) return;
      SpreadsheetModel *sm2 = nullptr;
      auto *m2 = model();
      if (auto *proxy = qobject_cast<QSortFilterProxyModel *>(m2)) {
        sm2 = qobject_cast<SpreadsheetModel *>(proxy->sourceModel());
      } else {
        sm2 = qobject_cast<SpreadsheetModel *>(m2);
      }
      if (!sm2) return;
      sm2->undoStack()->beginMacro(tr("Clear Cells"));
      for (const auto &i : sel) {
        int r = i.row(), c = i.column();
        if (auto *proxy = qobject_cast<QSortFilterProxyModel *>(m2)) {
          auto src = proxy->mapToSource(i);
          r = src.row();
          c = src.column();
        }
        sm2->set_cell_value(r, c, QVariant());
      }
      sm2->undoStack()->endMacro();
    });

    menu.addSeparator();
  }

  // Row / column operations.
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
