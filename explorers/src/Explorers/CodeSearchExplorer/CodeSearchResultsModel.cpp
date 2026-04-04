// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "CodeSearchResultsModel.h"

#include <multiplier/GUI/Interfaces/IModel.h>
#include <multiplier/GUI/Util.h>

Q_DECLARE_METATYPE(mx::TokenRange)
Q_DECLARE_METATYPE(mx::VariantEntity)

namespace mx::gui {

CodeSearchResultsModel::CodeSearchResultsModel(QObject *parent)
    : QAbstractTableModel(parent) {}

CodeSearchResultsModel::~CodeSearchResultsModel(void) {}

int CodeSearchResultsModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(rows.size());
}

int CodeSearchResultsModel::columnCount(const QModelIndex &parent) const {
  if (parent.isValid()) {
    return 0;
  }
  // Match (0), capture groups (1..N), File (last).
  return 2 + num_capture_columns;
}

QVariant CodeSearchResultsModel::headerData(
    int section, Qt::Orientation orientation, int role) const {
  if (orientation != Qt::Horizontal) {
    return {};
  }

  if (role == Qt::TextAlignmentRole) {
    return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
  }

  if (role != Qt::DisplayRole) {
    return {};
  }

  if (section == MatchColumn) {
    return tr("Match");
  } else if (section == LocationColumn()) {
    return tr("File");
  } else {
    int capture_index = section - FirstCaptureColumn;
    if (capture_index >= 0 && capture_index < capture_headers.size()) {
      return capture_headers[capture_index];
    }
    return tr("Group %1").arg(capture_index + 1);
  }
}

QVariant CodeSearchResultsModel::data(const QModelIndex &index,
                                      int role) const {
  if (!index.isValid()) {
    return {};
  }

  const int row = index.row();
  const int col = index.column();

  if (row < 0 || row >= rows.size()) {
    return {};
  }

  const auto &result = rows[row];
  const int loc_col = LocationColumn();

  if (role == Qt::DisplayRole) {
    if (col == MatchColumn) {
      return result.match_data;
    }
    if (col == loc_col) {
      if (!show_full_paths) {
        return ShortenLocation(result.location);
      }
      return result.location;
    }
    int capture_index = col - FirstCaptureColumn;
    if (capture_index >= 0 && capture_index < result.capture_data.size()) {
      return result.capture_data[capture_index];
    }
    return {};

  } else if (role == Qt::ToolTipRole) {
    if (col == loc_col) {
      return result.location;
    }
    return {};

  } else if (role == IModel::TokenRangeDisplayRole) {
    if (col == MatchColumn && !result.match_tokens.empty()) {
      return QVariant::fromValue(result.match_tokens);
    }
    int capture_index = col - FirstCaptureColumn;
    if (capture_index >= 0 &&
        capture_index < result.capture_token_ranges.size() &&
        !result.capture_token_ranges[capture_index].empty()) {
      return QVariant::fromValue(result.capture_token_ranges[capture_index]);
    }
    return {};

  } else if (role == IModel::EntityRole) {
    if (result.fragment) {
      return QVariant::fromValue(VariantEntity(result.fragment.value()));
    }
    if (result.file) {
      return QVariant::fromValue(VariantEntity(result.file.value()));
    }
    return {};

  } else if (role == IModel::ModelIdRole) {
    return QStringLiteral("com.trailofbits.explorer.CodeSearchExplorer");

  }

  return {};
}

int CodeSearchResultsModel::AppendRows(QVector<CodeSearchResultRow> new_rows) {
  if (new_rows.isEmpty()) {
    return static_cast<int>(rows.size());
  }

  // If this is the first batch, set up capture columns.
  if (rows.isEmpty() && !new_rows.isEmpty()) {
    const auto &first = new_rows.first();
    int new_capture_count = static_cast<int>(first.capture_data.size());

    if (new_capture_count != num_capture_columns) {
      beginResetModel();
      num_capture_columns = new_capture_count;
      capture_headers.clear();
      endResetModel();
    }
  }

  int first = static_cast<int>(rows.size());
  int last = first + static_cast<int>(new_rows.size()) - 1;
  beginInsertRows(QModelIndex(), first, last);
  rows.append(std::move(new_rows));
  endInsertRows();

  return static_cast<int>(rows.size());
}

void CodeSearchResultsModel::Clear(void) {
  if (rows.isEmpty()) {
    return;
  }
  beginResetModel();
  rows.clear();
  num_capture_columns = 0;
  capture_headers.clear();
  endResetModel();
}

const CodeSearchResultRow *CodeSearchResultsModel::Row(int row) const {
  if (row < 0 || row >= rows.size()) {
    return nullptr;
  }
  return &rows[row];
}

void CodeSearchResultsModel::SetShowFullPaths(bool show_full) {
  if (show_full_paths != show_full) {
    show_full_paths = show_full;
    int loc_col = LocationColumn();
    if (!rows.isEmpty()) {
      emit dataChanged(
          index(0, loc_col),
          index(static_cast<int>(rows.size()) - 1, loc_col),
          {Qt::DisplayRole, Qt::ToolTipRole});
    }
  }
}

bool CodeSearchResultsModel::GetShowFullPaths(void) const {
  return show_full_paths;
}

}  // namespace mx::gui
