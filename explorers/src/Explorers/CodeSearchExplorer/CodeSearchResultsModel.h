// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QAbstractTableModel>
#include <QString>
#include <QVector>

#include <optional>

#include <multiplier/Frontend/File.h>
#include <multiplier/Frontend/Token.h>
#include <multiplier/Fragment.h>

namespace mx::gui {

struct CodeSearchResultRow {
  // The matched text (capture group 0).
  QString match_data;

  // Syntax-highlighted token ranges for match and capture groups.
  TokenRange match_tokens;
  QVector<TokenRange> capture_token_ranges;

  // Capture groups 1..N as text.
  QVector<QString> capture_data;

  // Parsed tokens at the start of each capture group (for navigation).
  QVector<std::optional<Token>> capture_tokens;

  // The fragment and file containing the match.
  std::optional<Fragment> fragment;
  std::optional<File> file;

  // The file token at the start of the match (for navigation).
  std::optional<Token> match_token;

  // Full location string (path:line:col).
  QString location;
};

class CodeSearchResultsModel Q_DECL_FINAL : public QAbstractTableModel {
  Q_OBJECT

 public:
  // Column layout: Match (0), capture groups (1..N), File (last).
  static constexpr int MatchColumn = 0;
  static constexpr int FirstCaptureColumn = 1;

  // The location column index depends on the number of capture groups.
  int LocationColumn(void) const { return 1 + num_capture_columns; }

  explicit CodeSearchResultsModel(QObject *parent = nullptr);
  virtual ~CodeSearchResultsModel(void);

  int rowCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_FINAL;
  int columnCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_FINAL;

  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const Q_DECL_FINAL;

  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const Q_DECL_FINAL;

  // Append a batch of results. Returns the new total row count.
  int AppendRows(QVector<CodeSearchResultRow> new_rows);

  // Clear all results.
  void Clear(void);

  // Get the row data for a given row index.
  const CodeSearchResultRow *Row(int row) const;

  // File path display mode.
  void SetShowFullPaths(bool show_full);
  bool GetShowFullPaths(void) const;

 private:
  QVector<CodeSearchResultRow> rows;
  int num_capture_columns{0};
  QVector<QString> capture_headers;
  bool show_full_paths{false};
};

}  // namespace mx::gui

Q_DECLARE_METATYPE(mx::gui::CodeSearchResultRow)
