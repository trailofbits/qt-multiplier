/*
  Copyright (c) 2024-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QAbstractTableModel>
#include <QColor>
#include <QHash>
#include <QMetaType>
#include <QString>
#include <QUndoStack>
#include <QVariant>
#include <QVector>

#include <multiplier/Frontend/Token.h>

namespace mx::gui {

namespace SpreadsheetRoles {

// Offset from IModel roles to avoid collisions.
constexpr int RawValueRole = Qt::UserRole + 200;

}  // namespace SpreadsheetRoles

// A cell whose value is computed from a formula string.
struct FormulaCell {
  QString formula;
  QVariant cached_result;
  bool is_stale = true;
  QString error_message;
};

// A cell that references a nested document (rich text). The content lives
// in the gui_documents DB table, keyed by doc_id. Copying a cell copies
// the reference, not the content.
struct DocumentCell {
  int doc_id{-1};    // Primary key in gui_documents (-1 = not yet persisted).
  QString title;     // Cached document title, shown in the cell.
};


// Metadata for a single column.
struct ColumnDefinition {
  QString name;
  int logical_index;
};

// A table model that backs the spreadsheet view. Supports typed cells
// (QString, bool, Token, TokenRange, FormulaCell) with per-type editing
// and display behaviour.
class SetCellValueCommand;
class InsertRowsCommand;
class RemoveRowsCommand;
class InsertColumnsCommand;
class RemoveColumnsCommand;
class MoveRowCommand;
class MoveColumnCommand;

class SpreadsheetModel Q_DECL_FINAL : public QAbstractTableModel {
  Q_OBJECT

  friend class SetCellValueCommand;
  friend class InsertRowsCommand;
  friend class RemoveRowsCommand;
  friend class InsertColumnsCommand;
  friend class RemoveColumnsCommand;
  friend class MoveRowCommand;
  friend class MoveColumnCommand;

 public:
  explicit SpreadsheetModel(QObject *parent = nullptr);
  virtual ~SpreadsheetModel(void);

  // QAbstractTableModel overrides.
  int rowCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_FINAL;
  int columnCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_FINAL;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const Q_DECL_FINAL;
  bool setData(const QModelIndex &index, const QVariant &value,
               int role = Qt::EditRole) Q_DECL_FINAL;
  Qt::ItemFlags flags(const QModelIndex &index) const Q_DECL_FINAL;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const Q_DECL_FINAL;
  bool setHeaderData(int section, Qt::Orientation orientation,
                     const QVariant &value,
                     int role = Qt::EditRole) Q_DECL_FINAL;

  // Stable sort by column.
  void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) Q_DECL_FINAL;

  // Row / column mutation.
  bool insertRows(int row, int count,
                  const QModelIndex &parent = QModelIndex()) Q_DECL_FINAL;
  bool removeRows(int row, int count,
                  const QModelIndex &parent = QModelIndex()) Q_DECL_FINAL;
  bool insertColumns(int column, int count,
                     const QModelIndex &parent = QModelIndex()) Q_DECL_FINAL;
  bool removeColumns(int column, int count,
                     const QModelIndex &parent = QModelIndex()) Q_DECL_FINAL;

  // Move a single row from `from` to `to`.
  void move_row(int from, int to);

  // Move a single column from `from` to `to`.
  void move_column(int from, int to);

  // Replace the entire model contents from a list of column definitions and
  // row data. Emits beginResetModel / endResetModel.
  void populate_from_results(QVector<ColumnDefinition> columns,
                             QVector<QVector<QVariant>> rows);

  // Return a human-readable string for a cell value.
  static QString display_text_for(const QVariant &value);

  // Serialize/deserialize a cell value to/from a JSON string.
  // value_from_json takes an optional Index to resolve entity IDs
  // back to VariantEntity for highlight color support.
  static QString value_to_json(const QVariant &value);
  static QVariant value_from_json(const QString &json,
                                  const mx::Index *index = nullptr);

  // Set a single cell value (creates an undo command).
  void set_cell_value(int row, int col, const QVariant &value);

  // Access the undo stack for connecting to undo/redo actions.
  QUndoStack *undoStack(void) const;

  // Internal methods called by undo commands. These directly mutate data
  // and emit the appropriate model signals. Do not call directly unless
  // from an undo command.
  void set_cell_value_internal(int row, int col, const QVariant &value);
  void insert_rows_internal(int row, int count);
  void remove_rows_internal(int row, int count);
  void insert_columns_internal(int col, int count);
  void remove_columns_internal(int col, int count);
  void restore_columns_internal(int col,
                                const QVector<ColumnDefinition> &columns,
                                const QVector<QVector<QVariant>> &data);
  void move_row_internal(int from, int to);
  void move_column_internal(int from, int to);

  // Row/column background colors.
  void SetRowColor(int row, const QColor &color);
  void ClearRowColor(int row);
  void SetColumnColor(int col, const QColor &color);
  void ClearColumnColor(int col);
  QColor RowColor(int row) const;
  QColor ColumnColor(int col) const;

 private:
  QVector<ColumnDefinition> m_columns;
  QVector<QVector<QVariant>> m_rows;
  QUndoStack *m_undo_stack;
  QHash<int, QColor> m_row_colors;
  QHash<int, QColor> m_col_colors;
};

}  // namespace mx::gui

Q_DECLARE_METATYPE(mx::gui::FormulaCell)
Q_DECLARE_METATYPE(mx::gui::DocumentCell)
