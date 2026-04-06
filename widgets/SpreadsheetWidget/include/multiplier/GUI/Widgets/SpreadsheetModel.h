/*
  Copyright (c) 2024-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QAbstractTableModel>
#include <QMetaType>
#include <QString>
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

// Metadata for a single column.
struct ColumnDefinition {
  QString name;
  int logical_index;
};

// A table model that backs the spreadsheet view. Supports typed cells
// (QString, bool, Token, TokenRange, FormulaCell) with per-type editing
// and display behaviour.
class SpreadsheetModel Q_DECL_FINAL : public QAbstractTableModel {
  Q_OBJECT

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

 private:
  QVector<ColumnDefinition> m_columns;
  QVector<QVector<QVariant>> m_rows;
};

}  // namespace mx::gui

Q_DECLARE_METATYPE(mx::gui::FormulaCell)
