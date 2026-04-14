/*
  Copyright (c) 2024-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QUndoCommand>
#include <QVariant>
#include <QVector>

#include <multiplier/GUI/Widgets/SpreadsheetModel.h>

namespace mx::gui {

// Set a single cell value.
class SetCellValueCommand Q_DECL_FINAL : public QUndoCommand {
 public:
  SetCellValueCommand(SpreadsheetModel *model, int row, int col,
                      QVariant old_value, QVariant new_value,
                      QUndoCommand *parent = nullptr);

  void undo(void) Q_DECL_FINAL;
  void redo(void) Q_DECL_FINAL;

 private:
  SpreadsheetModel *m_model;
  int m_row;
  int m_col;
  QVariant m_old_value;
  QVariant m_new_value;
};

// Insert rows.
class InsertRowsCommand Q_DECL_FINAL : public QUndoCommand {
 public:
  InsertRowsCommand(SpreadsheetModel *model, int row, int count,
                    QUndoCommand *parent = nullptr);

  void undo(void) Q_DECL_FINAL;
  void redo(void) Q_DECL_FINAL;

 private:
  SpreadsheetModel *m_model;
  int m_row;
  int m_count;
};

// Remove rows, saving the data for undo.
class RemoveRowsCommand Q_DECL_FINAL : public QUndoCommand {
 public:
  RemoveRowsCommand(SpreadsheetModel *model, int row, int count,
                    QVector<QVector<QVariant>> saved_data,
                    QUndoCommand *parent = nullptr);

  void undo(void) Q_DECL_FINAL;
  void redo(void) Q_DECL_FINAL;

 private:
  SpreadsheetModel *m_model;
  int m_row;
  int m_count;
  QVector<QVector<QVariant>> m_saved_data;
};

// Insert columns.
class InsertColumnsCommand Q_DECL_FINAL : public QUndoCommand {
 public:
  InsertColumnsCommand(SpreadsheetModel *model, int col, int count,
                       QUndoCommand *parent = nullptr);

  void undo(void) Q_DECL_FINAL;
  void redo(void) Q_DECL_FINAL;

 private:
  SpreadsheetModel *m_model;
  int m_col;
  int m_count;
};

// Remove columns, saving column definitions and cell data for undo.
class RemoveColumnsCommand Q_DECL_FINAL : public QUndoCommand {
 public:
  RemoveColumnsCommand(SpreadsheetModel *model, int col, int count,
                       QVector<ColumnDefinition> saved_columns,
                       QVector<QVector<QVariant>> saved_data,
                       QUndoCommand *parent = nullptr);

  void undo(void) Q_DECL_FINAL;
  void redo(void) Q_DECL_FINAL;

 private:
  SpreadsheetModel *m_model;
  int m_col;
  int m_count;
  QVector<ColumnDefinition> m_saved_columns;
  QVector<QVector<QVariant>> m_saved_data;
};

// Move a single row.
class MoveRowCommand Q_DECL_FINAL : public QUndoCommand {
 public:
  MoveRowCommand(SpreadsheetModel *model, int from_row, int to_row,
                 QUndoCommand *parent = nullptr);

  void undo(void) Q_DECL_FINAL;
  void redo(void) Q_DECL_FINAL;

 private:
  SpreadsheetModel *m_model;
  int m_from_row;
  int m_to_row;
};

// Move a single column.
class MoveColumnCommand Q_DECL_FINAL : public QUndoCommand {
 public:
  MoveColumnCommand(SpreadsheetModel *model, int from_col, int to_col,
                    QUndoCommand *parent = nullptr);

  void undo(void) Q_DECL_FINAL;
  void redo(void) Q_DECL_FINAL;

 private:
  SpreadsheetModel *m_model;
  int m_from_col;
  int m_to_col;
};

}  // namespace mx::gui
