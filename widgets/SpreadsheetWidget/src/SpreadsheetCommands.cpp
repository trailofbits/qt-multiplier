// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "SpreadsheetCommands.h"

namespace mx::gui {

// ---------------------------------------------------------------------------
// SetCellValueCommand
// ---------------------------------------------------------------------------

SetCellValueCommand::SetCellValueCommand(SpreadsheetModel *model, int row,
                                         int col, QVariant old_value,
                                         QVariant new_value,
                                         QUndoCommand *parent)
    : QUndoCommand(parent),
      m_model(model),
      m_row(row),
      m_col(col),
      m_old_value(std::move(old_value)),
      m_new_value(std::move(new_value)) {
  setText(QStringLiteral("Set Cell (%1, %2)").arg(row).arg(col));
}

void SetCellValueCommand::undo(void) {
  m_model->set_cell_value_internal(m_row, m_col, m_old_value);
}

void SetCellValueCommand::redo(void) {
  m_model->set_cell_value_internal(m_row, m_col, m_new_value);
}

// ---------------------------------------------------------------------------
// InsertRowsCommand
// ---------------------------------------------------------------------------

InsertRowsCommand::InsertRowsCommand(SpreadsheetModel *model, int row,
                                     int count, QUndoCommand *parent)
    : QUndoCommand(parent), m_model(model), m_row(row), m_count(count) {
  setText(QStringLiteral("Insert %1 Row(s)").arg(count));
}

void InsertRowsCommand::undo(void) {
  m_model->remove_rows_internal(m_row, m_count);
}

void InsertRowsCommand::redo(void) {
  m_model->insert_rows_internal(m_row, m_count);
}

// ---------------------------------------------------------------------------
// RemoveRowsCommand
// ---------------------------------------------------------------------------

RemoveRowsCommand::RemoveRowsCommand(SpreadsheetModel *model, int row,
                                     int count,
                                     QVector<QVector<QVariant>> saved_data,
                                     QUndoCommand *parent)
    : QUndoCommand(parent),
      m_model(model),
      m_row(row),
      m_count(count),
      m_saved_data(std::move(saved_data)) {
  setText(QStringLiteral("Remove %1 Row(s)").arg(count));
}

void RemoveRowsCommand::undo(void) {
  m_model->insert_rows_internal(m_row, m_count);
  for (int i = 0; i < m_count; ++i) {
    for (int c = 0; c < m_saved_data[i].size(); ++c) {
      m_model->set_cell_value_internal(m_row + i, c, m_saved_data[i][c]);
    }
  }
}

void RemoveRowsCommand::redo(void) {
  m_model->remove_rows_internal(m_row, m_count);
}

// ---------------------------------------------------------------------------
// InsertColumnsCommand
// ---------------------------------------------------------------------------

InsertColumnsCommand::InsertColumnsCommand(SpreadsheetModel *model, int col,
                                           int count, QUndoCommand *parent)
    : QUndoCommand(parent), m_model(model), m_col(col), m_count(count) {
  setText(QStringLiteral("Insert %1 Column(s)").arg(count));
}

void InsertColumnsCommand::undo(void) {
  m_model->remove_columns_internal(m_col, m_count);
}

void InsertColumnsCommand::redo(void) {
  m_model->insert_columns_internal(m_col, m_count);
}

// ---------------------------------------------------------------------------
// RemoveColumnsCommand
// ---------------------------------------------------------------------------

RemoveColumnsCommand::RemoveColumnsCommand(
    SpreadsheetModel *model, int col, int count,
    QVector<ColumnDefinition> saved_columns,
    QVector<QVector<QVariant>> saved_data, QUndoCommand *parent)
    : QUndoCommand(parent),
      m_model(model),
      m_col(col),
      m_count(count),
      m_saved_columns(std::move(saved_columns)),
      m_saved_data(std::move(saved_data)) {
  setText(QStringLiteral("Remove %1 Column(s)").arg(count));
}

void RemoveColumnsCommand::undo(void) {
  m_model->insert_columns_internal(m_col, m_count);
  // Restore column definitions and cell data.
  m_model->restore_columns_internal(m_col, m_saved_columns, m_saved_data);
}

void RemoveColumnsCommand::redo(void) {
  m_model->remove_columns_internal(m_col, m_count);
}

// ---------------------------------------------------------------------------
// MoveRowCommand
// ---------------------------------------------------------------------------

MoveRowCommand::MoveRowCommand(SpreadsheetModel *model, int from_row,
                               int to_row, QUndoCommand *parent)
    : QUndoCommand(parent),
      m_model(model),
      m_from_row(from_row),
      m_to_row(to_row) {
  setText(QStringLiteral("Move Row %1 to %2").arg(from_row).arg(to_row));
}

void MoveRowCommand::undo(void) {
  m_model->move_row_internal(m_to_row, m_from_row);
}

void MoveRowCommand::redo(void) {
  m_model->move_row_internal(m_from_row, m_to_row);
}

// ---------------------------------------------------------------------------
// MoveColumnCommand
// ---------------------------------------------------------------------------

MoveColumnCommand::MoveColumnCommand(SpreadsheetModel *model, int from_col,
                                     int to_col, QUndoCommand *parent)
    : QUndoCommand(parent),
      m_model(model),
      m_from_col(from_col),
      m_to_col(to_col) {
  setText(QStringLiteral("Move Column %1 to %2").arg(from_col).arg(to_col));
}

void MoveColumnCommand::undo(void) {
  m_model->move_column_internal(m_to_col, m_from_col);
}

void MoveColumnCommand::redo(void) {
  m_model->move_column_internal(m_from_col, m_to_col);
}

}  // namespace mx::gui
