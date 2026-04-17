// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "SpreadsheetTools.h"

#include <QColor>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>

#include <multiplier/Frontend/File.h>
#include <multiplier/Index.h>
#include <multiplier/GUI/Util.h>
#include <multiplier/GUI/Widgets/SpreadsheetModel.h>

#include <algorithm>

namespace mx::gui {
namespace {

// Parse a color name or hex string to QColor.
static QColor parse_color(const QString &color) {
  static const QHash<QString, QColor> kNamedColors = {
    {QStringLiteral("red"),    QColor(Qt::red)},
    {QStringLiteral("orange"), QColor(255, 165, 0)},
    {QStringLiteral("yellow"), QColor(Qt::yellow)},
    {QStringLiteral("green"),  QColor(Qt::green)},
    {QStringLiteral("blue"),   QColor(Qt::blue)},
    {QStringLiteral("purple"), QColor(128, 0, 128)},
  };

  auto it = kNamedColors.find(color.toLower());
  if (it != kNamedColors.end()) {
    return it.value();
  }
  return QColor::fromString(color);
}

// Get display text for a cell JSON string. Deserializes and returns the
// display text.
static QString cell_display_text(const QString &cell_json) {
  QVariant v = SpreadsheetModel::value_from_json(cell_json);
  return SpreadsheetModel::display_text_for(v);
}

// Get the type string for a cell JSON string.
static QString cell_type_string(const QString &cell_json) {
  if (cell_json.isEmpty()) {
    return QStringLiteral("empty");
  }
  QJsonDocument doc = QJsonDocument::fromJson(cell_json.toUtf8());
  if (!doc.isObject()) {
    return QStringLiteral("string");
  }
  QString t = doc.object()[QStringLiteral("t")].toString();
  if (t == QLatin1String("b")) return QStringLiteral("bool");
  if (t == QLatin1String("s")) return QStringLiteral("string");
  if (t == QLatin1String("doc")) return QStringLiteral("document");
  if (t == QLatin1String("tok")) return QStringLiteral("token");
  if (t == QLatin1String("tr")) return QStringLiteral("token_range");
  if (t == QLatin1String("f")) return QStringLiteral("formula");
  return QStringLiteral("string");
}

// Build a JSON cell value from a string value and optional type hint.
static QString make_cell_json(const QString &value, const QString &type) {
  if (type == QLatin1String("bool")) {
    bool checked = (value == QLatin1String("true") ||
                    value == QLatin1String("1") ||
                    value == QLatin1String("yes"));
    return SpreadsheetModel::value_to_json(QVariant(checked));
  }
  if (type == QLatin1String("formula")) {
    // Store as a string starting with '='. The spreadsheet UI will
    // interpret it as a formula when loaded into the model.
    QString formula = value;
    if (!formula.startsWith(QLatin1Char('='))) {
      formula.prepend(QLatin1Char('='));
    }
    return SpreadsheetModel::value_to_json(QVariant(formula));
  }
  // Default: string.
  return SpreadsheetModel::value_to_json(QVariant(value));
}

// Convenience: get a cell value string from sheet data, empty if out of range.
static QString get_cell(const ConfigManager::SheetData &sheet, int row,
                        int col) {
  if (row < 0 || row >= sheet.cells.size()) return {};
  if (col < 0 || col >= sheet.columns.size()) return {};
  auto &r = sheet.cells[row];
  if (col >= r.size()) return {};
  return r[col];
}

// Ensure a cell slot exists in the sheet grid.
static void ensure_cell(ConfigManager::SheetData &sheet, int row, int col) {
  while (sheet.cells.size() <= row) {
    sheet.cells.append(QVector<QString>(sheet.columns.size()));
  }
  auto &r = sheet.cells[row];
  while (r.size() <= col) {
    r.append(QString());
  }
}

static QJsonObject error_result(const QString &msg) {
  QJsonObject r;
  r[QStringLiteral("error")] = msg;
  return r;
}

// Schema helpers.
static QJsonObject string_prop(const QString &desc) {
  QJsonObject p;
  p[QStringLiteral("type")] = QStringLiteral("string");
  p[QStringLiteral("description")] = desc;
  return p;
}

static QJsonObject int_prop(const QString &desc) {
  QJsonObject p;
  p[QStringLiteral("type")] = QStringLiteral("integer");
  p[QStringLiteral("description")] = desc;
  return p;
}

static QJsonObject bool_prop(const QString &desc) {
  QJsonObject p;
  p[QStringLiteral("type")] = QStringLiteral("boolean");
  p[QStringLiteral("description")] = desc;
  return p;
}

static QJsonObject string_array_prop(const QString &desc) {
  QJsonObject items;
  items[QStringLiteral("type")] = QStringLiteral("string");
  QJsonObject p;
  p[QStringLiteral("type")] = QStringLiteral("array");
  p[QStringLiteral("items")] = items;
  p[QStringLiteral("description")] = desc;
  return p;
}

static QJsonObject make_schema(const QJsonObject &properties,
                               const QJsonArray &required) {
  QJsonObject schema;
  schema[QStringLiteral("type")] = QStringLiteral("object");
  schema[QStringLiteral("properties")] = properties;
  if (!required.isEmpty()) {
    schema[QStringLiteral("required")] = required;
  }
  return schema;
}

// Build a LocationCell JSON string from an entity ID, resolving file path
// and line via the index.
static QString make_location_cell_json(ConfigManager *config,
                                       mx::RawEntityId entity_id) {
  const auto &index = config->Index();
  mx::VariantEntity vent = index.entity(mx::EntityId(entity_id));

  if (std::holds_alternative<mx::NotAnEntity>(vent)) {
    return {};
  }

  LocationCell lc;
  lc.entity_id = entity_id;

  if (auto loc = LocationOfEntityEx(config->FileLocationCache(), vent)) {
    lc.file_path = QString::fromStdString(loc->path.generic_string());
    lc.line = loc->line;
    lc.column = loc->column;
  }

  return SpreadsheetModel::value_to_json(QVariant::fromValue(lc));
}

// Try to interpret a string value as an entity reference.
// Returns a non-empty LocationCell JSON if the value looks like an entity
// reference ("entity:NNN" prefix or a pure number that resolves to a valid
// entity). Returns empty string otherwise.
static QString try_as_location_cell(ConfigManager *config,
                                    const QString &value) {
  bool is_entity_ref = false;
  mx::RawEntityId eid = 0;

  if (value.startsWith(QLatin1String("entity:"))) {
    bool ok = false;
    eid = static_cast<mx::RawEntityId>(
        value.mid(7).toLongLong(&ok));
    is_entity_ref = ok && eid != 0;
  } else {
    // Check if it's a pure number that resolves to a valid entity.
    bool ok = false;
    auto num = value.toLongLong(&ok);
    if (ok && num > 0) {
      eid = static_cast<mx::RawEntityId>(num);
      const auto &index = config->Index();
      mx::VariantEntity vent = index.entity(mx::EntityId(eid));
      is_entity_ref = !std::holds_alternative<mx::NotAnEntity>(vent);
    }
  }

  if (!is_entity_ref) {
    return {};
  }

  return make_location_cell_json(config, eid);
}

}  // namespace

// ===========================================================================
// CreateSheetTool
// ===========================================================================

QString CreateSheetTool::name(void) const {
  return QStringLiteral("create_sheet");
}

QString CreateSheetTool::description(void) const {
  return QStringLiteral(
      "Create a custom sheet. Prefer create_findings_sheet, "
      "create_attack_surface_sheet, or create_task for structured data.");
}

QJsonObject CreateSheetTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("name")] = string_prop(
      QStringLiteral("Name of the sheet"));
  props[QStringLiteral("columns")] = string_array_prop(
      QStringLiteral("List of column names"));
  props[QStringLiteral("description")] = string_prop(
      QStringLiteral("Optional description of the sheet"));
  props[QStringLiteral("role")] = string_prop(
      QStringLiteral("Optional role tag for the sheet (e.g. \"task_list\", "
                     "\"findings\"). Helps tools find sheets by purpose."));
  return make_schema(props, {QStringLiteral("name"),
                             QStringLiteral("columns")});
}

QJsonObject CreateSheetTool::execute(const QJsonObject &args) {
  ConfigManager::SheetData sheet;
  sheet.name = args[QStringLiteral("name")].toString();
  if (sheet.name.isEmpty()) {
    return error_result(QStringLiteral("name is required"));
  }

  QJsonArray cols = args[QStringLiteral("columns")].toArray();
  if (cols.isEmpty()) {
    return error_result(QStringLiteral("columns must be a non-empty array"));
  }

  for (const auto &c : cols) {
    ConfigManager::SheetColumnInfo ci;
    ci.name = c.toString();
    sheet.columns.append(ci);
  }

  sheet.description = args[QStringLiteral("description")].toString();
  sheet.role = args[QStringLiteral("role")].toString();

  int id = -1;
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    id = m_ctx->config->SaveSheet(sheet);
  }, Qt::BlockingQueuedConnection);
  QMetaObject::invokeMethod(m_ctx->config,
      &ConfigManager::NotifyExternalSheetsChanged, Qt::QueuedConnection);

  QJsonObject result;
  result[QStringLiteral("sheet_id")] = id;
  return result;
}

// ===========================================================================
// ListSheetsTool
// ===========================================================================

QString ListSheetsTool::name(void) const {
  return QStringLiteral("list_sheets");
}

QString ListSheetsTool::description(void) const {
  return QStringLiteral("List all open spreadsheets.");
}

QJsonObject ListSheetsTool::parametersSchema(void) const {
  return make_schema({}, {});
}

QJsonObject ListSheetsTool::execute(const QJsonObject &) {
  QVector<ConfigManager::SheetData> sheets;
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    sheets = m_ctx->config->LoadOpenSheets();
  }, Qt::BlockingQueuedConnection);
  QJsonArray arr;
  for (const auto &s : sheets) {
    QJsonObject entry;
    entry[QStringLiteral("sheet_id")] = s.sheet_id;
    entry[QStringLiteral("name")] = s.name;
    entry[QStringLiteral("description")] = s.description;
    arr.append(entry);
  }
  QJsonObject result;
  result[QStringLiteral("sheets")] = arr;
  return result;
}

// ===========================================================================
// GetSheetSummaryTool
// ===========================================================================

QString GetSheetSummaryTool::name(void) const {
  return QStringLiteral("get_sheet_summary");
}

QString GetSheetSummaryTool::description(void) const {
  return QStringLiteral("Get details about a sheet: name, description, "
                        "columns, and row count.");
}

QJsonObject GetSheetSummaryTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("sheet_id")] = int_prop(
      QStringLiteral("ID of the sheet"));
  return make_schema(props, {QStringLiteral("sheet_id")});
}

QJsonObject GetSheetSummaryTool::execute(const QJsonObject &args) {
  int sheet_id = args[QStringLiteral("sheet_id")].toInt(-1);
  if (sheet_id < 0) {
    return error_result(QStringLiteral("sheet_id is required"));
  }

  ConfigManager::SheetData sheet;
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    sheet = m_ctx->config->LoadSheetById(sheet_id);
  }, Qt::BlockingQueuedConnection);
  if (sheet.sheet_id < 0) {
    return error_result(QStringLiteral("sheet not found"));
  }

  QJsonArray col_names;
  for (const auto &c : sheet.columns) {
    col_names.append(c.name);
  }

  QJsonObject result;
  result[QStringLiteral("name")] = sheet.name;
  result[QStringLiteral("description")] = sheet.description;
  result[QStringLiteral("columns")] = col_names;
  result[QStringLiteral("row_count")] = sheet.cells.size();
  return result;
}

// ===========================================================================
// ReadCellTool
// ===========================================================================

QString ReadCellTool::name(void) const {
  return QStringLiteral("read_cell");
}

QString ReadCellTool::description(void) const {
  return QStringLiteral(
      "Read the value of a single cell. Row 0 is the first data row.");
}

QJsonObject ReadCellTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("sheet_id")] = int_prop(
      QStringLiteral("ID of the sheet"));
  props[QStringLiteral("row")] = int_prop(
      QStringLiteral("Row index (0-based)"));
  props[QStringLiteral("column")] = int_prop(
      QStringLiteral("Column index (0-based)"));
  return make_schema(props, {QStringLiteral("sheet_id"),
                             QStringLiteral("row"),
                             QStringLiteral("column")});
}

QJsonObject ReadCellTool::execute(const QJsonObject &args) {
  int sheet_id = args[QStringLiteral("sheet_id")].toInt(-1);
  int row = args[QStringLiteral("row")].toInt(-1);
  int col = args[QStringLiteral("column")].toInt(-1);

  ConfigManager::SheetData sheet;
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    sheet = m_ctx->config->LoadSheetById(sheet_id);
  }, Qt::BlockingQueuedConnection);
  if (sheet.sheet_id < 0) {
    return error_result(QStringLiteral("sheet not found"));
  }

  QString cell_json = get_cell(sheet, row, col);
  QJsonObject result;
  result[QStringLiteral("value")] = cell_display_text(cell_json);
  result[QStringLiteral("type")] = cell_type_string(cell_json);
  return result;
}

// ===========================================================================
// WriteCellTool
// ===========================================================================

QString WriteCellTool::name(void) const {
  return QStringLiteral("write_cell");
}

QString WriteCellTool::description(void) const {
  return QStringLiteral(
      "Write a value to a single cell. Row 0 is the first data row.");
}

QJsonObject WriteCellTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("sheet_id")] = int_prop(
      QStringLiteral("ID of the sheet"));
  props[QStringLiteral("row")] = int_prop(
      QStringLiteral("Row index (0-based)"));
  props[QStringLiteral("column")] = int_prop(
      QStringLiteral("Column index (0-based)"));
  props[QStringLiteral("value")] = string_prop(
      QStringLiteral("Value to write"));
  QJsonObject type_prop = string_prop(
      QStringLiteral("Cell type: \"string\", \"bool\", or \"formula\""));
  QJsonArray enum_values;
  enum_values.append(QStringLiteral("string"));
  enum_values.append(QStringLiteral("bool"));
  enum_values.append(QStringLiteral("formula"));
  type_prop[QStringLiteral("enum")] = enum_values;
  props[QStringLiteral("type")] = type_prop;
  return make_schema(props, {QStringLiteral("sheet_id"),
                             QStringLiteral("row"),
                             QStringLiteral("column"),
                             QStringLiteral("value")});
}

QJsonObject WriteCellTool::execute(const QJsonObject &args) {
  int sheet_id = args[QStringLiteral("sheet_id")].toInt(-1);
  int row = args[QStringLiteral("row")].toInt(-1);
  int col = args[QStringLiteral("column")].toInt(-1);
  QString value = args[QStringLiteral("value")].toString();
  QString type = args[QStringLiteral("type")].toString(
      QStringLiteral("string"));

  ConfigManager::SheetData sheet;
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    sheet = m_ctx->config->LoadSheetById(sheet_id);
  }, Qt::BlockingQueuedConnection);
  if (sheet.sheet_id < 0) {
    return error_result(QStringLiteral("sheet not found"));
  }
  if (col < 0 || col >= sheet.columns.size()) {
    return error_result(QStringLiteral("column out of range"));
  }

  ensure_cell(sheet, row, col);

  // Auto-detect entity references and create LocationCells instead of plain
  // strings when the value looks like an entity reference.
  QString cell_json;
  if (type == QLatin1String("string")) {
    cell_json = try_as_location_cell(m_ctx->config, value);
  }
  if (cell_json.isEmpty()) {
    cell_json = make_cell_json(value, type);
  }

  sheet.cells[row][col] = cell_json;
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    m_ctx->config->SaveSheet(sheet);
  }, Qt::BlockingQueuedConnection);
  QMetaObject::invokeMethod(m_ctx->config,
      &ConfigManager::NotifyExternalSheetsChanged, Qt::QueuedConnection);

  QJsonObject result;
  result[QStringLiteral("success")] = true;
  return result;
}

// ===========================================================================
// ReadRowTool
// ===========================================================================

QString ReadRowTool::name(void) const {
  return QStringLiteral("read_row");
}

QString ReadRowTool::description(void) const {
  return QStringLiteral(
      "Read all cells in a row. Row 0 is the first data row.");
}

QJsonObject ReadRowTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("sheet_id")] = int_prop(
      QStringLiteral("ID of the sheet"));
  props[QStringLiteral("row")] = int_prop(
      QStringLiteral("Row index (0-based)"));
  return make_schema(props, {QStringLiteral("sheet_id"),
                             QStringLiteral("row")});
}

QJsonObject ReadRowTool::execute(const QJsonObject &args) {
  int sheet_id = args[QStringLiteral("sheet_id")].toInt(-1);
  int row = args[QStringLiteral("row")].toInt(-1);

  ConfigManager::SheetData sheet;
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    sheet = m_ctx->config->LoadSheetById(sheet_id);
  }, Qt::BlockingQueuedConnection);
  if (sheet.sheet_id < 0) {
    return error_result(QStringLiteral("sheet not found"));
  }
  if (row < 0 || row >= sheet.cells.size()) {
    return error_result(QStringLiteral("row out of range"));
  }

  QJsonArray cells;
  for (int c = 0; c < sheet.columns.size(); ++c) {
    QString cj = get_cell(sheet, row, c);
    QJsonObject cell;
    cell[QStringLiteral("column")] = c;
    cell[QStringLiteral("value")] = cell_display_text(cj);
    cell[QStringLiteral("type")] = cell_type_string(cj);
    cells.append(cell);
  }

  QJsonObject result;
  result[QStringLiteral("cells")] = cells;
  return result;
}

// ===========================================================================
// ReadColumnTool
// ===========================================================================

QString ReadColumnTool::name(void) const {
  return QStringLiteral("read_column");
}

QString ReadColumnTool::description(void) const {
  return QStringLiteral("Read all cells in a column.");
}

QJsonObject ReadColumnTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("sheet_id")] = int_prop(
      QStringLiteral("ID of the sheet"));
  props[QStringLiteral("column")] = int_prop(
      QStringLiteral("Column index (0-based)"));
  return make_schema(props, {QStringLiteral("sheet_id"),
                             QStringLiteral("column")});
}

QJsonObject ReadColumnTool::execute(const QJsonObject &args) {
  int sheet_id = args[QStringLiteral("sheet_id")].toInt(-1);
  int col = args[QStringLiteral("column")].toInt(-1);

  ConfigManager::SheetData sheet;
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    sheet = m_ctx->config->LoadSheetById(sheet_id);
  }, Qt::BlockingQueuedConnection);
  if (sheet.sheet_id < 0) {
    return error_result(QStringLiteral("sheet not found"));
  }
  if (col < 0 || col >= sheet.columns.size()) {
    return error_result(QStringLiteral("column out of range"));
  }

  QJsonArray cells;
  for (int r = 0; r < sheet.cells.size(); ++r) {
    QString cj = get_cell(sheet, r, col);
    QJsonObject cell;
    cell[QStringLiteral("row")] = r;
    cell[QStringLiteral("value")] = cell_display_text(cj);
    cell[QStringLiteral("type")] = cell_type_string(cj);
    cells.append(cell);
  }

  QJsonObject result;
  result[QStringLiteral("cells")] = cells;
  return result;
}

// ===========================================================================
// AddRowTool
// ===========================================================================

QString AddRowTool::name(void) const {
  return QStringLiteral("add_row");
}

QString AddRowTool::description(void) const {
  return QStringLiteral(
      "Append a row to the end of the sheet. Returns the 0-based row_index "
      "of the new row. Row 0 is the first data row.");
}

QJsonObject AddRowTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("sheet_id")] = int_prop(
      QStringLiteral("ID of the sheet"));
  props[QStringLiteral("values")] = string_array_prop(
      QStringLiteral("Values for each column (as strings)"));
  return make_schema(props, {QStringLiteral("sheet_id"),
                             QStringLiteral("values")});
}

QJsonObject AddRowTool::execute(const QJsonObject &args) {
  int sheet_id = args[QStringLiteral("sheet_id")].toInt(-1);
  ConfigManager::SheetData sheet;
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    sheet = m_ctx->config->LoadSheetById(sheet_id);
  }, Qt::BlockingQueuedConnection);
  if (sheet.sheet_id < 0) {
    return error_result(QStringLiteral("sheet not found"));
  }

  QJsonArray vals = args[QStringLiteral("values")].toArray();
  QVector<QString> row;
  auto num_cols = sheet.columns.size();
  row.resize(num_cols);
  for (qsizetype i = 0; i < vals.size() && i < num_cols; ++i) {
    row[i] = make_cell_json(vals[i].toString(), QStringLiteral("string"));
  }

  auto row_index = sheet.cells.size();
  sheet.cells.append(row);
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    m_ctx->config->SaveSheet(sheet);
  }, Qt::BlockingQueuedConnection);
  QMetaObject::invokeMethod(m_ctx->config,
      &ConfigManager::NotifyExternalSheetsChanged, Qt::QueuedConnection);

  QJsonObject result;
  result[QStringLiteral("row_index")] = static_cast<int>(row_index);
  return result;
}

// ===========================================================================
// InsertRowTool
// ===========================================================================

QString InsertRowTool::name(void) const {
  return QStringLiteral("insert_row");
}

QString InsertRowTool::description(void) const {
  return QStringLiteral("Insert a row at a specific position.");
}

QJsonObject InsertRowTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("sheet_id")] = int_prop(
      QStringLiteral("ID of the sheet"));
  props[QStringLiteral("row")] = int_prop(
      QStringLiteral("Row index to insert at (0-based)"));
  props[QStringLiteral("values")] = string_array_prop(
      QStringLiteral("Optional values for each column"));
  return make_schema(props, {QStringLiteral("sheet_id"),
                             QStringLiteral("row")});
}

QJsonObject InsertRowTool::execute(const QJsonObject &args) {
  int sheet_id = args[QStringLiteral("sheet_id")].toInt(-1);
  int row = args[QStringLiteral("row")].toInt(-1);

  ConfigManager::SheetData sheet;
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    sheet = m_ctx->config->LoadSheetById(sheet_id);
  }, Qt::BlockingQueuedConnection);
  if (sheet.sheet_id < 0) {
    return error_result(QStringLiteral("sheet not found"));
  }
  if (row < 0 || row > sheet.cells.size()) {
    return error_result(QStringLiteral("row out of range"));
  }

  auto num_cols = sheet.columns.size();
  QVector<QString> new_row(num_cols);

  QJsonArray vals = args[QStringLiteral("values")].toArray();
  for (qsizetype i = 0; i < vals.size() && i < num_cols; ++i) {
    new_row[i] = make_cell_json(vals[i].toString(), QStringLiteral("string"));
  }

  sheet.cells.insert(row, new_row);

  // Shift row colors for rows at or after the insertion point.
  QHash<int, QColor> new_colors;
  for (auto it = sheet.row_colors.begin(); it != sheet.row_colors.end(); ++it) {
    if (it.key() >= row) {
      new_colors[it.key() + 1] = it.value();
    } else {
      new_colors[it.key()] = it.value();
    }
  }
  sheet.row_colors = new_colors;

  QMetaObject::invokeMethod(m_ctx->config, [&] {
    m_ctx->config->SaveSheet(sheet);
  }, Qt::BlockingQueuedConnection);
  QMetaObject::invokeMethod(m_ctx->config,
      &ConfigManager::NotifyExternalSheetsChanged, Qt::QueuedConnection);

  QJsonObject result;
  result[QStringLiteral("row_index")] = row;
  return result;
}

// ===========================================================================
// DeleteRowTool
// ===========================================================================

QString DeleteRowTool::name(void) const {
  return QStringLiteral("delete_row");
}

QString DeleteRowTool::description(void) const {
  return QStringLiteral("Delete a row from the sheet.");
}

QJsonObject DeleteRowTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("sheet_id")] = int_prop(
      QStringLiteral("ID of the sheet"));
  props[QStringLiteral("row")] = int_prop(
      QStringLiteral("Row index to delete (0-based)"));
  return make_schema(props, {QStringLiteral("sheet_id"),
                             QStringLiteral("row")});
}

QJsonObject DeleteRowTool::execute(const QJsonObject &args) {
  int sheet_id = args[QStringLiteral("sheet_id")].toInt(-1);
  int row = args[QStringLiteral("row")].toInt(-1);

  ConfigManager::SheetData sheet;
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    sheet = m_ctx->config->LoadSheetById(sheet_id);
  }, Qt::BlockingQueuedConnection);
  if (sheet.sheet_id < 0) {
    return error_result(QStringLiteral("sheet not found"));
  }
  if (row < 0 || row >= sheet.cells.size()) {
    return error_result(QStringLiteral("row out of range"));
  }

  sheet.cells.removeAt(row);

  // Shift row colors.
  QHash<int, QColor> new_colors;
  for (auto it = sheet.row_colors.begin(); it != sheet.row_colors.end(); ++it) {
    if (it.key() == row) {
      continue;  // Removed row.
    } else if (it.key() > row) {
      new_colors[it.key() - 1] = it.value();
    } else {
      new_colors[it.key()] = it.value();
    }
  }
  sheet.row_colors = new_colors;

  QMetaObject::invokeMethod(m_ctx->config, [&] {
    m_ctx->config->SaveSheet(sheet);
  }, Qt::BlockingQueuedConnection);
  QMetaObject::invokeMethod(m_ctx->config,
      &ConfigManager::NotifyExternalSheetsChanged, Qt::QueuedConnection);

  QJsonObject result;
  result[QStringLiteral("success")] = true;
  return result;
}

// ===========================================================================
// AddColumnTool
// ===========================================================================

QString AddColumnTool::name(void) const {
  return QStringLiteral("add_column");
}

QString AddColumnTool::description(void) const {
  return QStringLiteral("Add a new column to the sheet.");
}

QJsonObject AddColumnTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("sheet_id")] = int_prop(
      QStringLiteral("ID of the sheet"));
  props[QStringLiteral("name")] = string_prop(
      QStringLiteral("Name of the new column"));
  return make_schema(props, {QStringLiteral("sheet_id"),
                             QStringLiteral("name")});
}

QJsonObject AddColumnTool::execute(const QJsonObject &args) {
  int sheet_id = args[QStringLiteral("sheet_id")].toInt(-1);
  QString col_name = args[QStringLiteral("name")].toString();

  ConfigManager::SheetData sheet;
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    sheet = m_ctx->config->LoadSheetById(sheet_id);
  }, Qt::BlockingQueuedConnection);
  if (sheet.sheet_id < 0) {
    return error_result(QStringLiteral("sheet not found"));
  }

  ConfigManager::SheetColumnInfo ci;
  ci.name = col_name;
  auto col_index = sheet.columns.size();
  sheet.columns.append(ci);

  // Extend each row to have a cell for the new column.
  for (auto &r : sheet.cells) {
    r.append(QString());
  }

  QMetaObject::invokeMethod(m_ctx->config, [&] {
    m_ctx->config->SaveSheet(sheet);
  }, Qt::BlockingQueuedConnection);
  QMetaObject::invokeMethod(m_ctx->config,
      &ConfigManager::NotifyExternalSheetsChanged, Qt::QueuedConnection);

  QJsonObject result;
  result[QStringLiteral("column_index")] = static_cast<int>(col_index);
  return result;
}

// ===========================================================================
// SetRowColorTool
// ===========================================================================

QString SetRowColorTool::name(void) const {
  return QStringLiteral("set_row_color");
}

QString SetRowColorTool::description(void) const {
  return QStringLiteral("Set the background color of a row.");
}

QJsonObject SetRowColorTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("sheet_id")] = int_prop(
      QStringLiteral("ID of the sheet"));
  props[QStringLiteral("row")] = int_prop(
      QStringLiteral("Row index (0-based)"));
  props[QStringLiteral("color")] = string_prop(
      QStringLiteral("Color name (red, orange, yellow, green, blue, purple) "
                     "or hex (#RRGGBB)"));
  return make_schema(props, {QStringLiteral("sheet_id"),
                             QStringLiteral("row"),
                             QStringLiteral("color")});
}

QJsonObject SetRowColorTool::execute(const QJsonObject &args) {
  int sheet_id = args[QStringLiteral("sheet_id")].toInt(-1);
  int row = args[QStringLiteral("row")].toInt(-1);
  QString color_str = args[QStringLiteral("color")].toString();

  ConfigManager::SheetData sheet;
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    sheet = m_ctx->config->LoadSheetById(sheet_id);
  }, Qt::BlockingQueuedConnection);
  if (sheet.sheet_id < 0) {
    return error_result(QStringLiteral("sheet not found"));
  }
  if (row < 0 || row >= sheet.cells.size()) {
    return error_result(QStringLiteral("row out of range"));
  }

  QColor color = parse_color(color_str);
  if (!color.isValid()) {
    return error_result(QStringLiteral("invalid color: ") + color_str);
  }

  sheet.row_colors[row] = color;
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    m_ctx->config->SaveSheet(sheet);
  }, Qt::BlockingQueuedConnection);
  QMetaObject::invokeMethod(m_ctx->config,
      &ConfigManager::NotifyExternalSheetsChanged, Qt::QueuedConnection);

  QJsonObject result;
  result[QStringLiteral("success")] = true;
  return result;
}

// ===========================================================================
// ClearRowColorTool
// ===========================================================================

QString ClearRowColorTool::name(void) const {
  return QStringLiteral("clear_row_color");
}

QString ClearRowColorTool::description(void) const {
  return QStringLiteral("Clear the background color of a row.");
}

QJsonObject ClearRowColorTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("sheet_id")] = int_prop(
      QStringLiteral("ID of the sheet"));
  props[QStringLiteral("row")] = int_prop(
      QStringLiteral("Row index (0-based)"));
  return make_schema(props, {QStringLiteral("sheet_id"),
                             QStringLiteral("row")});
}

QJsonObject ClearRowColorTool::execute(const QJsonObject &args) {
  int sheet_id = args[QStringLiteral("sheet_id")].toInt(-1);
  int row = args[QStringLiteral("row")].toInt(-1);

  ConfigManager::SheetData sheet;
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    sheet = m_ctx->config->LoadSheetById(sheet_id);
  }, Qt::BlockingQueuedConnection);
  if (sheet.sheet_id < 0) {
    return error_result(QStringLiteral("sheet not found"));
  }

  sheet.row_colors.remove(row);
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    m_ctx->config->SaveSheet(sheet);
  }, Qt::BlockingQueuedConnection);
  QMetaObject::invokeMethod(m_ctx->config,
      &ConfigManager::NotifyExternalSheetsChanged, Qt::QueuedConnection);

  QJsonObject result;
  result[QStringLiteral("success")] = true;
  return result;
}

// ===========================================================================
// SetCheckboxTool
// ===========================================================================

QString SetCheckboxTool::name(void) const {
  return QStringLiteral("set_checkbox");
}

QString SetCheckboxTool::description(void) const {
  return QStringLiteral("Set a boolean/checkbox value in a cell.");
}

QJsonObject SetCheckboxTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("sheet_id")] = int_prop(
      QStringLiteral("ID of the sheet"));
  props[QStringLiteral("row")] = int_prop(
      QStringLiteral("Row index (0-based)"));
  props[QStringLiteral("column")] = int_prop(
      QStringLiteral("Column index (0-based)"));
  props[QStringLiteral("checked")] = bool_prop(
      QStringLiteral("Whether the checkbox is checked"));
  return make_schema(props, {QStringLiteral("sheet_id"),
                             QStringLiteral("row"),
                             QStringLiteral("column"),
                             QStringLiteral("checked")});
}

QJsonObject SetCheckboxTool::execute(const QJsonObject &args) {
  int sheet_id = args[QStringLiteral("sheet_id")].toInt(-1);
  int row = args[QStringLiteral("row")].toInt(-1);
  int col = args[QStringLiteral("column")].toInt(-1);
  bool checked = args[QStringLiteral("checked")].toBool();

  ConfigManager::SheetData sheet;
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    sheet = m_ctx->config->LoadSheetById(sheet_id);
  }, Qt::BlockingQueuedConnection);
  if (sheet.sheet_id < 0) {
    return error_result(QStringLiteral("sheet not found"));
  }
  if (col < 0 || col >= sheet.columns.size()) {
    return error_result(QStringLiteral("column out of range"));
  }

  ensure_cell(sheet, row, col);
  sheet.cells[row][col] = SpreadsheetModel::value_to_json(QVariant(checked));
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    m_ctx->config->SaveSheet(sheet);
  }, Qt::BlockingQueuedConnection);
  QMetaObject::invokeMethod(m_ctx->config,
      &ConfigManager::NotifyExternalSheetsChanged, Qt::QueuedConnection);

  QJsonObject result;
  result[QStringLiteral("success")] = true;
  return result;
}

// ===========================================================================
// SortSheetTool
// ===========================================================================

QString SortSheetTool::name(void) const {
  return QStringLiteral("sort_sheet");
}

QString SortSheetTool::description(void) const {
  return QStringLiteral("Sort all rows in a sheet by a column.");
}

QJsonObject SortSheetTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("sheet_id")] = int_prop(
      QStringLiteral("ID of the sheet"));
  props[QStringLiteral("column")] = int_prop(
      QStringLiteral("Column index to sort by (0-based)"));
  QJsonObject order_prop = string_prop(
      QStringLiteral("Sort order: \"asc\" or \"desc\" (default: \"asc\")"));
  QJsonArray order_enum;
  order_enum.append(QStringLiteral("asc"));
  order_enum.append(QStringLiteral("desc"));
  order_prop[QStringLiteral("enum")] = order_enum;
  props[QStringLiteral("order")] = order_prop;
  return make_schema(props, {QStringLiteral("sheet_id"),
                             QStringLiteral("column")});
}

QJsonObject SortSheetTool::execute(const QJsonObject &args) {
  int sheet_id = args[QStringLiteral("sheet_id")].toInt(-1);
  int col = args[QStringLiteral("column")].toInt(-1);
  QString order = args[QStringLiteral("order")].toString(
      QStringLiteral("asc"));

  ConfigManager::SheetData sheet;
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    sheet = m_ctx->config->LoadSheetById(sheet_id);
  }, Qt::BlockingQueuedConnection);
  if (sheet.sheet_id < 0) {
    return error_result(QStringLiteral("sheet not found"));
  }
  if (col < 0 || col >= sheet.columns.size()) {
    return error_result(QStringLiteral("column out of range"));
  }

  bool ascending = (order != QLatin1String("desc"));

  // Build index array for stable sort so we can remap row colors.
  QVector<int> indices(sheet.cells.size());
  std::iota(indices.begin(), indices.end(), 0);

  std::stable_sort(indices.begin(), indices.end(),
                   [&](int a, int b) {
    QString va = cell_display_text(get_cell(sheet, a, col));
    QString vb = cell_display_text(get_cell(sheet, b, col));
    if (ascending) {
      return va.compare(vb, Qt::CaseInsensitive) < 0;
    }
    return va.compare(vb, Qt::CaseInsensitive) > 0;
  });

  QVector<QVector<QString>> sorted_cells;
  sorted_cells.reserve(sheet.cells.size());
  QHash<int, QColor> sorted_colors;

  for (int i = 0; i < indices.size(); ++i) {
    sorted_cells.append(sheet.cells[indices[i]]);
    if (sheet.row_colors.contains(indices[i])) {
      sorted_colors[i] = sheet.row_colors[indices[i]];
    }
  }

  sheet.cells = sorted_cells;
  sheet.row_colors = sorted_colors;
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    m_ctx->config->SaveSheet(sheet);
  }, Qt::BlockingQueuedConnection);
  QMetaObject::invokeMethod(m_ctx->config,
      &ConfigManager::NotifyExternalSheetsChanged, Qt::QueuedConnection);

  QJsonObject result;
  result[QStringLiteral("success")] = true;
  return result;
}

// ===========================================================================
// ReadSheetRangeTool
// ===========================================================================

QString ReadSheetRangeTool::name(void) const {
  return QStringLiteral("read_sheet_range");
}

QString ReadSheetRangeTool::description(void) const {
  return QStringLiteral("Read a rectangular range of cells.");
}

QJsonObject ReadSheetRangeTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("sheet_id")] = int_prop(
      QStringLiteral("ID of the sheet"));
  props[QStringLiteral("start_row")] = int_prop(
      QStringLiteral("Start row (0-based, inclusive)"));
  props[QStringLiteral("start_col")] = int_prop(
      QStringLiteral("Start column (0-based, inclusive)"));
  props[QStringLiteral("end_row")] = int_prop(
      QStringLiteral("End row (0-based, inclusive)"));
  props[QStringLiteral("end_col")] = int_prop(
      QStringLiteral("End column (0-based, inclusive)"));
  return make_schema(props, {QStringLiteral("sheet_id"),
                             QStringLiteral("start_row"),
                             QStringLiteral("start_col"),
                             QStringLiteral("end_row"),
                             QStringLiteral("end_col")});
}

QJsonObject ReadSheetRangeTool::execute(const QJsonObject &args) {
  int sheet_id = args[QStringLiteral("sheet_id")].toInt(-1);
  int start_row = args[QStringLiteral("start_row")].toInt(-1);
  int start_col = args[QStringLiteral("start_col")].toInt(-1);
  int end_row = args[QStringLiteral("end_row")].toInt(-1);
  int end_col = args[QStringLiteral("end_col")].toInt(-1);

  ConfigManager::SheetData sheet;
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    sheet = m_ctx->config->LoadSheetById(sheet_id);
  }, Qt::BlockingQueuedConnection);
  if (sheet.sheet_id < 0) {
    return error_result(QStringLiteral("sheet not found"));
  }

  QJsonArray rows;
  for (int r = start_row; r <= end_row && r < sheet.cells.size(); ++r) {
    if (r < 0) continue;
    QJsonArray row;
    for (int c = start_col; c <= end_col && c < sheet.columns.size(); ++c) {
      if (c < 0) continue;
      row.append(cell_display_text(get_cell(sheet, r, c)));
    }
    rows.append(row);
  }

  QJsonObject result;
  result[QStringLiteral("cells")] = rows;
  return result;
}

// ===========================================================================
// GetSheetAsMarkdownTool
// ===========================================================================

QString GetSheetAsMarkdownTool::name(void) const {
  return QStringLiteral("get_sheet_as_markdown");
}

QString GetSheetAsMarkdownTool::description(void) const {
  return QStringLiteral("Export a sheet as a markdown table.");
}

QJsonObject GetSheetAsMarkdownTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("sheet_id")] = int_prop(
      QStringLiteral("ID of the sheet"));
  return make_schema(props, {QStringLiteral("sheet_id")});
}

QJsonObject GetSheetAsMarkdownTool::execute(const QJsonObject &args) {
  int sheet_id = args[QStringLiteral("sheet_id")].toInt(-1);

  ConfigManager::SheetData sheet;
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    sheet = m_ctx->config->LoadSheetById(sheet_id);
  }, Qt::BlockingQueuedConnection);
  if (sheet.sheet_id < 0) {
    return error_result(QStringLiteral("sheet not found"));
  }

  auto num_cols = sheet.columns.size();
  if (num_cols == 0) {
    QJsonObject result;
    result[QStringLiteral("markdown")] = QStringLiteral("(empty sheet)");
    return result;
  }

  // Compute column widths.
  QVector<qsizetype> widths(num_cols);
  for (qsizetype c = 0; c < num_cols; ++c) {
    widths[c] = sheet.columns[c].name.size();
  }
  for (const auto &row : sheet.cells) {
    for (qsizetype c = 0; c < num_cols && c < row.size(); ++c) {
      auto len = cell_display_text(row[c]).size();
      if (len > widths[c]) widths[c] = len;
    }
  }
  // Minimum width of 3 for the separator.
  for (qsizetype c = 0; c < num_cols; ++c) {
    if (widths[c] < 3) widths[c] = 3;
  }

  QString md;

  // Header row.
  md += QLatin1Char('|');
  for (qsizetype c = 0; c < num_cols; ++c) {
    md += QLatin1Char(' ');
    md += sheet.columns[c].name.leftJustified(static_cast<int>(widths[c]));
    md += QStringLiteral(" |");
  }
  md += QLatin1Char('\n');

  // Separator row.
  md += QLatin1Char('|');
  for (qsizetype c = 0; c < num_cols; ++c) {
    md += QLatin1Char(' ');
    md += QString(static_cast<int>(widths[c]), QLatin1Char('-'));
    md += QStringLiteral(" |");
  }
  md += QLatin1Char('\n');

  // Data rows.
  for (const auto &row : sheet.cells) {
    md += QLatin1Char('|');
    for (qsizetype c = 0; c < num_cols; ++c) {
      QString text;
      if (c < row.size()) {
        text = cell_display_text(row[c]);
      }
      md += QLatin1Char(' ');
      md += text.leftJustified(static_cast<int>(widths[c]));
      md += QStringLiteral(" |");
    }
    md += QLatin1Char('\n');
  }

  QJsonObject result;
  result[QStringLiteral("markdown")] = md;
  return result;
}

// ===========================================================================
// WriteLocationCellTool
// ===========================================================================

QString WriteLocationCellTool::name(void) const {
  return QStringLiteral("write_location_cell");
}

QString WriteLocationCellTool::description(void) const {
  return QStringLiteral(
      "Write a clickable code location into a sheet cell. The cell will "
      "display as 'file:line' and navigate to the entity on click.");
}

QJsonObject WriteLocationCellTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("sheet_id")] = int_prop(
      QStringLiteral("ID of the sheet"));
  props[QStringLiteral("row")] = int_prop(
      QStringLiteral("Row index (0-based)"));
  props[QStringLiteral("column")] = int_prop(
      QStringLiteral("Column index (0-based)"));

  QJsonObject eid_prop;
  eid_prop[QStringLiteral("type")] = QStringLiteral("string");
  eid_prop[QStringLiteral("description")] =
      QStringLiteral("Entity ID as a string to navigate to on click");
  props[QStringLiteral("entity_id")] = eid_prop;

  return make_schema(props, {QStringLiteral("sheet_id"),
                             QStringLiteral("row"),
                             QStringLiteral("column"),
                             QStringLiteral("entity_id")});
}

QJsonObject WriteLocationCellTool::execute(const QJsonObject &args) {
  int sheet_id = args[QStringLiteral("sheet_id")].toInt(-1);
  int row = args[QStringLiteral("row")].toInt(-1);
  int col = args[QStringLiteral("column")].toInt(-1);
  auto eid_val = args[QStringLiteral("entity_id")];
  mx::RawEntityId entity_id;
  if (eid_val.isString()) {
    entity_id = eid_val.toString().toULongLong();
  } else {
    entity_id = static_cast<mx::RawEntityId>(eid_val.toDouble(0));
  }

  if (entity_id == 0) {
    return error_result(QStringLiteral("entity_id is required"));
  }

  ConfigManager::SheetData sheet;
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    sheet = m_ctx->config->LoadSheetById(sheet_id);
  }, Qt::BlockingQueuedConnection);
  if (sheet.sheet_id < 0) {
    return error_result(QStringLiteral("sheet not found"));
  }
  if (col < 0 || col >= sheet.columns.size()) {
    return error_result(QStringLiteral("column out of range"));
  }

  QString cell_json = make_location_cell_json(m_ctx->config, entity_id);
  if (cell_json.isEmpty()) {
    return error_result(QStringLiteral("entity not found for given entity_id"));
  }

  ensure_cell(sheet, row, col);
  sheet.cells[row][col] = cell_json;

  QMetaObject::invokeMethod(m_ctx->config, [&] {
    m_ctx->config->SaveSheet(sheet);
  }, Qt::BlockingQueuedConnection);
  QMetaObject::invokeMethod(m_ctx->config,
      &ConfigManager::NotifyExternalSheetsChanged, Qt::QueuedConnection);

  QJsonObject result;
  result[QStringLiteral("success")] = true;
  return result;
}

// ===========================================================================
// CreateFindingsSheetTool
// ===========================================================================

QString CreateFindingsSheetTool::name(void) const {
  return QStringLiteral("create_findings_sheet");
}

QString CreateFindingsSheetTool::description(void) const {
  return QStringLiteral(
      "Create a structured findings sheet for recording analysis results.");
}

QJsonObject CreateFindingsSheetTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("name")] = string_prop(
      QStringLiteral("Name of the sheet (default: \"Findings\")"));
  props[QStringLiteral("description")] = string_prop(
      QStringLiteral("Optional description of the findings sheet"));
  return make_schema(props, {});
}

QJsonObject CreateFindingsSheetTool::execute(const QJsonObject &args) {
  ConfigManager::SheetData sheet;
  sheet.name = args[QStringLiteral("name")].toString();
  if (sheet.name.isEmpty()) {
    sheet.name = QStringLiteral("Findings");
  }
  sheet.description = args[QStringLiteral("description")].toString();
  sheet.role = QStringLiteral("findings");

  // Location (clickable).
  {
    ConfigManager::SheetColumnInfo ci;
    ci.name = QStringLiteral("Location");
    ci.clickable = true;
    sheet.columns.append(ci);
  }
  // Finding.
  {
    ConfigManager::SheetColumnInfo ci;
    ci.name = QStringLiteral("Finding");
    sheet.columns.append(ci);
  }
  // Severity.
  {
    ConfigManager::SheetColumnInfo ci;
    ci.name = QStringLiteral("Severity");
    sheet.columns.append(ci);
  }
  // Evidence (document link).
  {
    ConfigManager::SheetColumnInfo ci;
    ci.name = QStringLiteral("Evidence");
    sheet.columns.append(ci);
  }
  // Status.
  {
    ConfigManager::SheetColumnInfo ci;
    ci.name = QStringLiteral("Status");
    sheet.columns.append(ci);
  }
  // Notes.
  {
    ConfigManager::SheetColumnInfo ci;
    ci.name = QStringLiteral("Notes");
    sheet.columns.append(ci);
  }

  int id = -1;
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    id = m_ctx->config->SaveSheet(sheet);
  }, Qt::BlockingQueuedConnection);
  QMetaObject::invokeMethod(m_ctx->config,
      &ConfigManager::NotifyExternalSheetsChanged, Qt::QueuedConnection);

  QJsonObject result;
  result[QStringLiteral("sheet_id")] = id;
  result[QStringLiteral("columns")] = QJsonArray({
      QStringLiteral("Location"),
      QStringLiteral("Finding"),
      QStringLiteral("Severity"),
      QStringLiteral("Evidence"),
      QStringLiteral("Status"),
      QStringLiteral("Notes")});
  return result;
}

// ===========================================================================
// CreateAttackSurfaceSheetTool
// ===========================================================================

QString CreateAttackSurfaceSheetTool::name(void) const {
  return QStringLiteral("create_attack_surface_sheet");
}

QString CreateAttackSurfaceSheetTool::description(void) const {
  return QStringLiteral(
      "Create a structured sheet for mapping attack surface entry points.");
}

QJsonObject CreateAttackSurfaceSheetTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("name")] = string_prop(
      QStringLiteral("Name of the sheet (default: \"Attack Surface\")"));
  return make_schema(props, {});
}

QJsonObject CreateAttackSurfaceSheetTool::execute(const QJsonObject &args) {
  ConfigManager::SheetData sheet;
  sheet.name = args[QStringLiteral("name")].toString();
  if (sheet.name.isEmpty()) {
    sheet.name = QStringLiteral("Attack Surface");
  }
  sheet.role = QStringLiteral("attack_surface");

  // Entry Point (clickable location).
  {
    ConfigManager::SheetColumnInfo ci;
    ci.name = QStringLiteral("Entry Point");
    ci.clickable = true;
    sheet.columns.append(ci);
  }
  // Type.
  {
    ConfigManager::SheetColumnInfo ci;
    ci.name = QStringLiteral("Type");
    sheet.columns.append(ci);
  }
  // Data Format.
  {
    ConfigManager::SheetColumnInfo ci;
    ci.name = QStringLiteral("Data Format");
    sheet.columns.append(ci);
  }
  // Validation.
  {
    ConfigManager::SheetColumnInfo ci;
    ci.name = QStringLiteral("Validation");
    sheet.columns.append(ci);
  }
  // Priority.
  {
    ConfigManager::SheetColumnInfo ci;
    ci.name = QStringLiteral("Priority");
    sheet.columns.append(ci);
  }
  // Notes.
  {
    ConfigManager::SheetColumnInfo ci;
    ci.name = QStringLiteral("Notes");
    sheet.columns.append(ci);
  }

  int id = -1;
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    id = m_ctx->config->SaveSheet(sheet);
  }, Qt::BlockingQueuedConnection);
  QMetaObject::invokeMethod(m_ctx->config,
      &ConfigManager::NotifyExternalSheetsChanged, Qt::QueuedConnection);

  QJsonObject result;
  result[QStringLiteral("sheet_id")] = id;
  result[QStringLiteral("columns")] = QJsonArray({
      QStringLiteral("Entry Point"),
      QStringLiteral("Type"),
      QStringLiteral("Data Format"),
      QStringLiteral("Validation"),
      QStringLiteral("Priority"),
      QStringLiteral("Notes")});
  return result;
}

// ===========================================================================
// Registration
// ===========================================================================

// ===========================================================================
// GetCellFormatReferenceTool
// ===========================================================================

static const QString kCellRefTitle =
    QStringLiteral("Cell Content Format Reference");

static QString loadCellRefFromResource(void) {
  QFile f(QStringLiteral(":/agent/references/cell_format.md"));
  if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return QString::fromUtf8(f.readAll());
  }
  return {};
}

QString GetCellFormatReferenceTool::name(void) const {
  return QStringLiteral("get_cell_format_reference");
}

QString GetCellFormatReferenceTool::description(void) const {
  return QStringLiteral(
      "Get a reference guide for spreadsheet cell content types: strings, "
      "checkboxes, clickable locations, document links, and tokens.");
}

QJsonObject GetCellFormatReferenceTool::parametersSchema(void) const {
  return make_schema({}, {});
}

QJsonObject GetCellFormatReferenceTool::execute(const QJsonObject &) {
  // Find or create the reference document.
  QVector<ConfigManager::DocumentInfo> docs;
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    docs = m_ctx->config->LoadDocumentsByCategory(QStringLiteral("reference"));
  }, Qt::BlockingQueuedConnection);

  for (const auto &doc : docs) {
    if (doc.title == kCellRefTitle) {
      QString content;
      QMetaObject::invokeMethod(m_ctx->config, [&] {
        content = m_ctx->config->LoadDocumentContent(doc.doc_id);
      }, Qt::BlockingQueuedConnection);
      QJsonObject result;
      result[QStringLiteral("reference")] = content;
      return result;
    }
  }

  // Create it from the resource file.
  auto resource_content = loadCellRefFromResource();
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    auto doc_id = m_ctx->config->CreateDocument(resource_content, kCellRefTitle,
                                                QStringLiteral("markdown"));
    if (doc_id >= 0) {
      m_ctx->config->SetDocumentCategory(doc_id, QStringLiteral("reference"));
    }
  }, Qt::BlockingQueuedConnection);

  QJsonObject result;
  result[QStringLiteral("reference")] = resource_content;
  return result;
}

void registerSpreadsheetTools(AgentToolRegistry &registry,
                              SpreadsheetToolContext *ctx) {
  registry.registerTool(std::make_unique<CreateSheetTool>(ctx));
  registry.registerTool(std::make_unique<ListSheetsTool>(ctx));
  registry.registerTool(std::make_unique<GetSheetSummaryTool>(ctx));
  registry.registerTool(std::make_unique<ReadCellTool>(ctx));
  registry.registerTool(std::make_unique<WriteCellTool>(ctx));
  registry.registerTool(std::make_unique<WriteLocationCellTool>(ctx));
  registry.registerTool(std::make_unique<ReadRowTool>(ctx));
  registry.registerTool(std::make_unique<ReadColumnTool>(ctx));
  registry.registerTool(std::make_unique<AddRowTool>(ctx));
  registry.registerTool(std::make_unique<InsertRowTool>(ctx));
  registry.registerTool(std::make_unique<DeleteRowTool>(ctx));
  registry.registerTool(std::make_unique<AddColumnTool>(ctx));
  registry.registerTool(std::make_unique<SetRowColorTool>(ctx));
  registry.registerTool(std::make_unique<ClearRowColorTool>(ctx));
  registry.registerTool(std::make_unique<SetCheckboxTool>(ctx));
  registry.registerTool(std::make_unique<SortSheetTool>(ctx));
  registry.registerTool(std::make_unique<ReadSheetRangeTool>(ctx));
  registry.registerTool(std::make_unique<GetSheetAsMarkdownTool>(ctx));
  registry.registerTool(std::make_unique<CreateFindingsSheetTool>(ctx));
  registry.registerTool(std::make_unique<CreateAttackSurfaceSheetTool>(ctx));
  registry.registerTool(std::make_unique<GetCellFormatReferenceTool>(ctx));
}

}  // namespace mx::gui
