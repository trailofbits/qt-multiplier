// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Widgets/SpreadsheetModel.h>

#include <QFont>
#include <QUndoStack>

#include <multiplier/Frontend/Token.h>

#include "SpreadsheetCommands.h"

namespace mx::gui {

SpreadsheetModel::SpreadsheetModel(QObject *parent)
    : QAbstractTableModel(parent),
      m_undo_stack(new QUndoStack(this)) {}

SpreadsheetModel::~SpreadsheetModel(void) {}

int SpreadsheetModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(m_rows.size());
}

int SpreadsheetModel::columnCount(const QModelIndex &parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(m_columns.size());
}

QVariant SpreadsheetModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid()) {
    return {};
  }

  int row = index.row();
  int col = index.column();
  if (row < 0 || row >= m_rows.size() || col < 0 || col >= m_columns.size()) {
    return {};
  }

  const QVariant &cell = m_rows[row][col];

  switch (role) {
    case SpreadsheetRoles::RawValueRole:
      return cell;

    case Qt::DisplayRole: {
      return display_text_for(cell);
    }

    case Qt::EditRole: {
      if (cell.canConvert<FormulaCell>()) {
        return cell.value<FormulaCell>().formula;
      }
      if (cell.canConvert<QString>()) {
        return cell.toString();
      }
      return {};
    }

    case Qt::CheckStateRole: {
      if (cell.userType() == QMetaType::Bool) {
        return cell.toBool() ? Qt::Checked : Qt::Unchecked;
      }
      return {};
    }

    case Qt::FontRole: {
      if (cell.canConvert<FormulaCell>()) {
        QFont font;
        font.setItalic(true);
        return font;
      }
      return {};
    }

    default:
      break;
  }

  return {};
}

bool SpreadsheetModel::setData(const QModelIndex &index, const QVariant &value,
                               int role) {
  if (!index.isValid()) {
    return false;
  }

  int row = index.row();
  int col = index.column();
  if (row < 0 || row >= m_rows.size() || col < 0 || col >= m_columns.size()) {
    return false;
  }

  if (role == Qt::CheckStateRole) {
    const QVariant &cell = m_rows[row][col];
    if (cell.userType() == QMetaType::Bool) {
      QVariant old_val = cell;
      QVariant new_val = QVariant(value.toInt() == Qt::Checked);
      m_undo_stack->push(
          new SetCellValueCommand(this, row, col, old_val, new_val));
      return true;
    }
    return false;
  }

  if (role != Qt::EditRole) {
    return false;
  }

  QVariant old_val = m_rows[row][col];
  QString text = value.toString();

  QVariant new_val;
  // Detect formula mode: text starting with '='.
  if (text.startsWith(QLatin1Char('='))) {
    FormulaCell fc;
    fc.formula = text;
    fc.cached_result = QVariant();
    fc.is_stale = true;
    new_val = QVariant::fromValue(fc);
  } else {
    new_val = QVariant(text);
  }

  m_undo_stack->push(
      new SetCellValueCommand(this, row, col, old_val, new_val));
  return true;
}

Qt::ItemFlags SpreadsheetModel::flags(const QModelIndex &index) const {
  if (!index.isValid()) {
    return Qt::NoItemFlags;
  }

  Qt::ItemFlags base = Qt::ItemIsEnabled | Qt::ItemIsSelectable;

  int row = index.row();
  int col = index.column();
  if (row < 0 || row >= m_rows.size() || col < 0 || col >= m_columns.size()) {
    return base;
  }

  const QVariant &cell = m_rows[row][col];

  // Token and TokenRange cells are read-only.
  if (cell.canConvert<Token>() || cell.canConvert<TokenRange>()) {
    return base;
  }

  // Bool cells are checkable but not text-editable.
  if (cell.userType() == QMetaType::Bool) {
    return base | Qt::ItemIsUserCheckable;
  }

  // QString and FormulaCell are editable.
  if (cell.canConvert<QString>() || cell.canConvert<FormulaCell>()) {
    return base | Qt::ItemIsEditable;
  }

  return base;
}

QVariant SpreadsheetModel::headerData(int section, Qt::Orientation orientation,
                                      int role) const {
  if (role != Qt::DisplayRole) {
    return {};
  }

  if (orientation == Qt::Horizontal) {
    if (section >= 0 && section < m_columns.size()) {
      return m_columns[section].name;
    }
  } else {
    // Row headers are 1-based indices.
    return section + 1;
  }

  return {};
}

bool SpreadsheetModel::insertRows(int row, int count,
                                  const QModelIndex &parent) {
  if (parent.isValid() || count <= 0) {
    return false;
  }

  row = qBound(0, row, static_cast<int>(m_rows.size()));
  m_undo_stack->push(new InsertRowsCommand(this, row, count));
  return true;
}

bool SpreadsheetModel::removeRows(int row, int count,
                                  const QModelIndex &parent) {
  if (parent.isValid() || count <= 0 || row < 0 ||
      row + count > m_rows.size()) {
    return false;
  }

  // Save the row data before removing so undo can restore it.
  QVector<QVector<QVariant>> saved_data;
  saved_data.reserve(count);
  for (int i = 0; i < count; ++i) {
    saved_data.append(m_rows[row + i]);
  }
  m_undo_stack->push(
      new RemoveRowsCommand(this, row, count, std::move(saved_data)));
  return true;
}

bool SpreadsheetModel::insertColumns(int column, int count,
                                     const QModelIndex &parent) {
  if (parent.isValid() || count <= 0) {
    return false;
  }

  column = qBound(0, column, static_cast<int>(m_columns.size()));
  m_undo_stack->push(new InsertColumnsCommand(this, column, count));
  return true;
}

bool SpreadsheetModel::removeColumns(int column, int count,
                                     const QModelIndex &parent) {
  if (parent.isValid() || count <= 0 || column < 0 ||
      column + count > m_columns.size()) {
    return false;
  }

  // Save column definitions and per-row cell data for undo.
  QVector<ColumnDefinition> saved_columns = m_columns.mid(column, count);
  QVector<QVector<QVariant>> saved_data;
  saved_data.reserve(m_rows.size());
  for (const auto &row : m_rows) {
    saved_data.append(row.mid(column, count));
  }
  m_undo_stack->push(new RemoveColumnsCommand(this, column, count,
                                              std::move(saved_columns),
                                              std::move(saved_data)));
  return true;
}

void SpreadsheetModel::move_row(int from, int to) {
  if (from < 0 || from >= m_rows.size() || to < 0 || to >= m_rows.size() ||
      from == to) {
    return;
  }

  m_undo_stack->push(new MoveRowCommand(this, from, to));
}

void SpreadsheetModel::move_column(int from, int to) {
  if (from < 0 || from >= m_columns.size() || to < 0 ||
      to >= m_columns.size() || from == to) {
    return;
  }

  m_undo_stack->push(new MoveColumnCommand(this, from, to));
}

// ---------------------------------------------------------------------------
// Internal methods (called by undo commands, not public API)
// ---------------------------------------------------------------------------

void SpreadsheetModel::set_cell_value_internal(int row, int col,
                                               const QVariant &value) {
  if (row < 0 || row >= m_rows.size() || col < 0 || col >= m_columns.size()) {
    return;
  }
  m_rows[row][col] = value;
  QModelIndex idx = index(row, col);
  emit dataChanged(idx, idx,
                   {Qt::DisplayRole, Qt::EditRole, Qt::CheckStateRole});
}

void SpreadsheetModel::insert_rows_internal(int row, int count) {
  int cols = static_cast<int>(m_columns.size());
  beginInsertRows(QModelIndex(), row, row + count - 1);
  for (int i = 0; i < count; ++i) {
    m_rows.insert(row, QVector<QVariant>(cols));
  }
  endInsertRows();
}

void SpreadsheetModel::remove_rows_internal(int row, int count) {
  beginRemoveRows(QModelIndex(), row, row + count - 1);
  m_rows.remove(row, count);
  endRemoveRows();
}

void SpreadsheetModel::insert_columns_internal(int col, int count) {
  beginInsertColumns(QModelIndex(), col, col + count - 1);
  for (int i = 0; i < count; ++i) {
    ColumnDefinition def;
    def.name = QString("Column %1").arg(m_columns.size() + 1);
    def.logical_index = static_cast<int>(m_columns.size());
    m_columns.insert(col + i, def);
  }
  for (auto &row : m_rows) {
    for (int i = 0; i < count; ++i) {
      row.insert(col, QVariant());
    }
  }
  endInsertColumns();
}

void SpreadsheetModel::remove_columns_internal(int col, int count) {
  beginRemoveColumns(QModelIndex(), col, col + count - 1);
  m_columns.remove(col, count);
  for (auto &row : m_rows) {
    row.remove(col, count);
  }
  endRemoveColumns();
}

void SpreadsheetModel::restore_columns_internal(
    int col, const QVector<ColumnDefinition> &columns,
    const QVector<QVector<QVariant>> &data) {
  // Overwrite column definitions that were just inserted with the saved ones.
  for (int i = 0; i < columns.size(); ++i) {
    m_columns[col + i] = columns[i];
  }
  // Restore per-row cell data.
  for (int r = 0; r < data.size() && r < m_rows.size(); ++r) {
    for (int c = 0; c < data[r].size(); ++c) {
      m_rows[r][col + c] = data[r][c];
    }
  }
  // Notify views that header and cell data changed.
  if (!columns.isEmpty()) {
    emit headerDataChanged(Qt::Horizontal, col,
                           col + static_cast<int>(columns.size()) - 1);
  }
  if (!m_rows.isEmpty() && !columns.isEmpty()) {
    emit dataChanged(index(0, col),
                     index(static_cast<int>(m_rows.size()) - 1,
                           col + static_cast<int>(columns.size()) - 1));
  }
}

void SpreadsheetModel::move_row_internal(int from, int to) {
  if (from < 0 || from >= m_rows.size() || to < 0 || to >= m_rows.size() ||
      from == to) {
    return;
  }

  int dest = (to > from) ? to + 1 : to;
  if (!beginMoveRows(QModelIndex(), from, from, QModelIndex(), dest)) {
    return;
  }

  QVector<QVariant> row = m_rows.takeAt(from);
  m_rows.insert(to, row);
  endMoveRows();
}

void SpreadsheetModel::move_column_internal(int from, int to) {
  if (from < 0 || from >= m_columns.size() || to < 0 ||
      to >= m_columns.size() || from == to) {
    return;
  }

  int dest = (to > from) ? to + 1 : to;
  if (!beginMoveColumns(QModelIndex(), from, from, QModelIndex(), dest)) {
    return;
  }

  ColumnDefinition col_def = m_columns.takeAt(from);
  m_columns.insert(to, col_def);

  for (auto &row : m_rows) {
    QVariant cell = row.takeAt(from);
    row.insert(to, cell);
  }
  endMoveColumns();
}

QUndoStack *SpreadsheetModel::undoStack(void) const {
  return m_undo_stack;
}

void SpreadsheetModel::set_cell_value(int row, int col,
                                      const QVariant &value) {
  if (row < 0 || row >= m_rows.size() || col < 0 || col >= m_columns.size()) {
    return;
  }
  QVariant old_val = m_rows[row][col];
  m_undo_stack->push(
      new SetCellValueCommand(this, row, col, old_val, value));
}

void SpreadsheetModel::populate_from_results(
    QVector<ColumnDefinition> columns, QVector<QVector<QVariant>> rows) {
  beginResetModel();
  m_columns = std::move(columns);
  m_rows = std::move(rows);
  endResetModel();
}

QString SpreadsheetModel::display_text_for(const QVariant &value) {
  if (!value.isValid()) {
    return {};
  }

  if (value.canConvert<FormulaCell>()) {
    const auto fc = value.value<FormulaCell>();
    if (!fc.error_message.isEmpty()) {
      return QStringLiteral("#ERR: %1").arg(fc.error_message);
    }
    if (fc.is_stale || !fc.cached_result.isValid()) {
      return fc.formula;
    }
    return fc.cached_result.toString();
  }

  if (value.canConvert<Token>()) {
    auto tok = value.value<Token>();
    return QString::fromStdString(std::string(tok.data()));
  }

  if (value.canConvert<TokenRange>()) {
    auto range = value.value<TokenRange>();
    QString result;
    for (auto tok : range) {
      result += QString::fromStdString(std::string(tok.data()));
    }
    return result;
  }

  if (value.userType() == QMetaType::Bool) {
    return value.toBool() ? QStringLiteral("true")
                          : QStringLiteral("false");
  }

  return value.toString();
}

}  // namespace mx::gui
