// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Widgets/SpreadsheetModel.h>

#include <QFont>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUndoStack>

#include <algorithm>

#include <multiplier/Frontend/Token.h>
#include <multiplier/Index.h>

#include "SpreadsheetCommands.h"

Q_DECLARE_METATYPE(mx::Token)
Q_DECLARE_METATYPE(mx::TokenRange)
Q_DECLARE_METATYPE(mx::UserToken)

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
      // Bool cells: empty text — the checkbox widget is the visual.
      if (cell.userType() == QMetaType::Bool) {
        return {};
      }
      return display_text_for(cell);
    }

    case Qt::EditRole: {
      if (cell.canConvert<FormulaCell>()) {
        return cell.value<FormulaCell>().formula;
      }
      // For all cell types (including Token/TokenRange), return the
      // display text for editing. If the user doesn't change it, we
      // preserve the original value (no downgrade).
      return display_text_for(cell);
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

    case Qt::TextAlignmentRole: {
      if (cell.userType() == QMetaType::Bool) {
        return static_cast<int>(Qt::AlignCenter);
      }
      return {};
    }

    case Qt::BackgroundRole: {
      // Row color takes priority, then column color.
      auto row_it = m_row_colors.find(row);
      if (row_it != m_row_colors.end()) {
        return row_it.value();
      }
      auto col_it = m_col_colors.find(col);
      if (col_it != m_col_colors.end()) {
        return col_it.value();
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
  QString old_text = display_text_for(old_val);

  // If the text didn't change, preserve the original value (no downgrade
  // from Token/TokenRange to QString).
  if (text == old_text) {
    return false;
  }

  QVariant new_val;

  // Detect special cell types and formula mode.
  if (text.compare(QStringLiteral("=doc"), Qt::CaseInsensitive) == 0) {
    DocumentCell dc;
    // doc_id stays -1; the viewer will allocate an ID when opened.
    new_val = QVariant::fromValue(dc);

  } else if (text.startsWith(QLatin1Char('='))) {
    FormulaCell fc;
    fc.formula = text;
    fc.cached_result = QVariant();
    fc.is_stale = true;
    new_val = QVariant::fromValue(fc);

  // If the old value was a TokenRange and the edit is a pure substring
  // (prefix/suffix trimmed), try to preserve token structure.
  } else if (old_val.canConvert<TokenRange>() &&
             old_text.contains(text) && !text.isEmpty()) {
    auto range = old_val.value<TokenRange>();
    qsizetype trim_start = old_text.indexOf(text);
    qsizetype trim_end = trim_start + text.size();

    // Walk tokens and find which ones overlap [trim_start, trim_end).
    std::vector<CustomToken> kept;
    qsizetype pos = 0;
    for (Token tok : range) {
      auto tok_data = tok.data();
      QString tok_text = QString::fromUtf8(
          tok_data.data(), static_cast<qsizetype>(tok_data.size()));
      qsizetype tok_start = pos;
      qsizetype tok_end = pos + tok_text.size();
      pos = tok_end;

      if (tok_end <= trim_start || tok_start >= trim_end) {
        continue;  // Token entirely outside the kept range.
      }

      // Compute overlap.
      qsizetype overlap_start = std::max(tok_start, trim_start);
      qsizetype overlap_end = std::min(tok_end, trim_end);
      bool fully_covered = (overlap_start == tok_start &&
                            overlap_end == tok_end);

      if (fully_covered) {
        kept.emplace_back(std::move(tok));
      } else {
        // Partial token — create a UserToken with the substring.
        QString sub = tok_text.mid(
            static_cast<qsizetype>(overlap_start - tok_start),
            static_cast<qsizetype>(overlap_end - overlap_start));
        UserToken ut;
        ut.data = sub.toStdString();
        ut.kind = tok.kind();
        ut.category = tok.category();
        ut.related_entity = tok.related_entity();
        kept.emplace_back(std::move(ut));
      }
    }

    if (!kept.empty()) {
      new_val = QVariant::fromValue(TokenRange::create(std::move(kept)));
    } else {
      new_val = QVariant(text);
    }

  } else {
    new_val = QVariant(text);
  }

  m_undo_stack->push(
      new SetCellValueCommand(this, row, col, old_val, new_val));
  return true;
}

void SpreadsheetModel::sort(int column, Qt::SortOrder order) {
  if (column < 0 || column >= m_columns.size() || m_rows.isEmpty()) {
    return;
  }

  emit layoutAboutToBeChanged();

  std::stable_sort(m_rows.begin(), m_rows.end(),
      [column, order] (const QVector<QVariant> &a,
                       const QVector<QVariant> &b) {
        QString at = (column < a.size())
            ? display_text_for(a[column]) : QString();
        QString bt = (column < b.size())
            ? display_text_for(b[column]) : QString();
        if (order == Qt::AscendingOrder) {
          return at.compare(bt, Qt::CaseInsensitive) < 0;
        }
        return bt.compare(at, Qt::CaseInsensitive) < 0;
      });

  emit layoutChanged();
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

  // All cells are editable — token cells downgrade to text on edit.
  // (Token/TokenRange cells also support copy & paste of rich data.)

  // Bool cells are checkable but not text-editable.
  if (cell.userType() == QMetaType::Bool) {
    return base | Qt::ItemIsUserCheckable;
  }

  // Empty cells, QString, and FormulaCell are editable.
  if (!cell.isValid() || cell.canConvert<QString>() ||
      cell.canConvert<FormulaCell>()) {
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

bool SpreadsheetModel::setHeaderData(int section, Qt::Orientation orientation,
                                     const QVariant &value, int role) {
  if (role != Qt::EditRole || orientation != Qt::Horizontal) {
    return false;
  }
  if (section < 0 || section >= m_columns.size()) {
    return false;
  }
  m_columns[section].name = value.toString();
  emit headerDataChanged(Qt::Horizontal, section, section);
  return true;
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

  if (value.canConvert<DocumentCell>()) {
    auto dc = value.value<DocumentCell>();
    return dc.title.isEmpty()
        ? QStringLiteral("[doc:%1]").arg(dc.doc_id)
        : dc.title;
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
    // Strip trailing whitespace so row height isn't inflated.
    while (result.endsWith(QLatin1Char('\n')) ||
           result.endsWith(QLatin1Char('\r'))) {
      result.chop(1);
    }
    return result;
  }

  if (value.userType() == QMetaType::Bool) {
    return value.toBool() ? QStringLiteral("true")
                          : QStringLiteral("false");
  }

  return value.toString();
}

void SpreadsheetModel::SetRowColor(int row, const QColor &color) {
  m_row_colors[row] = color;
  if (m_columns.size() > 0) {
    emit dataChanged(index(row, 0),
                     index(row, static_cast<int>(m_columns.size()) - 1),
                     {Qt::BackgroundRole});
  }
}

void SpreadsheetModel::ClearRowColor(int row) {
  m_row_colors.remove(row);
  if (m_columns.size() > 0) {
    emit dataChanged(index(row, 0),
                     index(row, static_cast<int>(m_columns.size()) - 1),
                     {Qt::BackgroundRole});
  }
}

void SpreadsheetModel::SetColumnColor(int col, const QColor &color) {
  m_col_colors[col] = color;
  if (m_rows.size() > 0) {
    emit dataChanged(index(0, col),
                     index(static_cast<int>(m_rows.size()) - 1, col),
                     {Qt::BackgroundRole});
  }
}

void SpreadsheetModel::ClearColumnColor(int col) {
  m_col_colors.remove(col);
  if (m_rows.size() > 0) {
    emit dataChanged(index(0, col),
                     index(static_cast<int>(m_rows.size()) - 1, col),
                     {Qt::BackgroundRole});
  }
}

QColor SpreadsheetModel::RowColor(int row) const {
  auto it = m_row_colors.find(row);
  return (it != m_row_colors.end()) ? it.value() : QColor();
}

QColor SpreadsheetModel::ColumnColor(int col) const {
  auto it = m_col_colors.find(col);
  return (it != m_col_colors.end()) ? it.value() : QColor();
}

QString SpreadsheetModel::value_to_json(const QVariant &value) {
  if (!value.isValid()) {
    return {};
  }

  if (value.userType() == QMetaType::Bool) {
    return value.toBool() ? QStringLiteral("{\"t\":\"b\",\"v\":1}")
                          : QStringLiteral("{\"t\":\"b\",\"v\":0}");
  }

  if (value.canConvert<DocumentCell>()) {
    auto dc = value.value<DocumentCell>();
    return QStringLiteral("{\"t\":\"doc\",\"id\":%1,\"tl\":\"%2\"}")
        .arg(dc.doc_id)
        .arg(dc.title.replace(QLatin1Char('"'), QStringLiteral("\\\"")));
  }

  if (value.canConvert<TokenRange>()) {
    // Serialize tokens as JSON array of {kind, category, data}.
    auto range = value.value<TokenRange>();
    QString json = QStringLiteral("{\"t\":\"tr\",\"v\":[");
    bool first = true;
    for (Token tok : range) {
      if (!first) json += QLatin1Char(',');
      first = false;
      auto td = tok.data();
      QString data = QString::fromUtf8(td.data(),
                                       static_cast<qsizetype>(td.size()));
      // Escape quotes and backslashes in data.
      data.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
      data.replace(QLatin1Char('"'), QStringLiteral("\\\""));
      auto eid = tok.related_entity_id().Pack();
      json += QStringLiteral("{\"k\":%1,\"c\":%2,\"d\":\"%3\",\"e\":%4}")
          .arg(static_cast<int>(tok.kind()))
          .arg(static_cast<int>(tok.category()))
          .arg(data)
          .arg(static_cast<qint64>(eid));
    }
    json += QStringLiteral("]}");
    return json;
  }

  if (value.canConvert<Token>()) {
    auto tok = value.value<Token>();
    auto td = tok.data();
    QString data = QString::fromUtf8(td.data(),
                                     static_cast<qsizetype>(td.size()));
    data.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    data.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    auto eid = tok.related_entity_id().Pack();
    return QStringLiteral("{\"t\":\"tok\",\"k\":%1,\"c\":%2,\"d\":\"%3\",\"e\":%4}")
        .arg(static_cast<int>(tok.kind()))
        .arg(static_cast<int>(tok.category()))
        .arg(data)
        .arg(static_cast<qint64>(eid));
  }

  // Default: string.
  QString text = value.toString();
  text.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
  text.replace(QLatin1Char('"'), QStringLiteral("\\\""));
  return QStringLiteral("{\"t\":\"s\",\"v\":\"%1\"}").arg(text);
}

QVariant SpreadsheetModel::value_from_json(const QString &json,
                                            const mx::Index *index) {
  if (json.isEmpty()) {
    return {};
  }

  QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
  if (!doc.isObject()) {
    // Legacy: treat as plain string if not JSON.
    return json;
  }

  QJsonObject obj = doc.object();
  QString type = obj[QStringLiteral("t")].toString();

  if (type == QLatin1String("b")) {
    return obj[QStringLiteral("v")].toInt() != 0;
  }

  if (type == QLatin1String("s")) {
    return obj[QStringLiteral("v")].toString();
  }

  if (type == QLatin1String("doc")) {
    DocumentCell dc;
    dc.doc_id = obj[QStringLiteral("id")].toInt(-1);
    dc.title = obj[QStringLiteral("tl")].toString();
    return QVariant::fromValue(dc);
  }

  if (type == QLatin1String("tok")) {
    UserToken ut;
    ut.kind = static_cast<TokenKind>(obj[QStringLiteral("k")].toInt());
    ut.category = static_cast<TokenCategory>(obj[QStringLiteral("c")].toInt());
    ut.data = obj[QStringLiteral("d")].toString().toStdString();
    auto eid = static_cast<RawEntityId>(
        obj[QStringLiteral("e")].toDouble());
    if (eid != kInvalidEntityId && index) {
      ut.related_entity = index->entity(EntityId(eid));
    }
    std::vector<CustomToken> toks;
    toks.emplace_back(std::move(ut));
    return QVariant::fromValue(TokenRange::create(std::move(toks)));
  }

  if (type == QLatin1String("tr")) {
    auto arr = obj[QStringLiteral("v")].toArray();
    std::vector<CustomToken> toks;
    for (const auto &elem : arr) {
      auto to = elem.toObject();
      UserToken ut;
      ut.kind = static_cast<TokenKind>(to[QStringLiteral("k")].toInt());
      ut.category = static_cast<TokenCategory>(to[QStringLiteral("c")].toInt());
      ut.data = to[QStringLiteral("d")].toString().toStdString();
      auto eid = static_cast<RawEntityId>(
          to[QStringLiteral("e")].toDouble());
      if (eid != kInvalidEntityId && index) {
        ut.related_entity = index->entity(EntityId(eid));
      }
      toks.emplace_back(std::move(ut));
    }
    if (!toks.empty()) {
      return QVariant::fromValue(TokenRange::create(std::move(toks)));
    }
    return {};
  }

  return {};
}

}  // namespace mx::gui
