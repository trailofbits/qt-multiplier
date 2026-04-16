// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "TaskTools.h"

#include <QColor>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>

#include <multiplier/GUI/Widgets/SpreadsheetModel.h>

namespace mx::gui {
namespace {

// Column indices for the task sheet schema.
enum TaskCol : int {
  kColId = 0,
  kColDescription,
  kColStatus,
  kColPriority,
  kColEntity,
  kColNotes,
  kColDone,
  kNumCols
};

static const QStringList kTaskColumnNames = {
    QStringLiteral("ID"),
    QStringLiteral("Description"),
    QStringLiteral("Status"),
    QStringLiteral("Priority"),
    QStringLiteral("Entity"),
    QStringLiteral("Notes"),
    QStringLiteral("Done")};

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

// Build a JSON cell value from a string.
static QString make_string_cell(const QString &value) {
  return SpreadsheetModel::value_to_json(QVariant(value));
}

// Build a JSON cell value from a bool.
static QString make_bool_cell(bool value) {
  return SpreadsheetModel::value_to_json(QVariant(value));
}

// Get display text for a cell JSON string.
static QString cell_display_text(const QString &cell_json) {
  QVariant v = SpreadsheetModel::value_from_json(cell_json);
  return SpreadsheetModel::display_text_for(v);
}

// Get the color for a priority level.
static QColor priority_color(const QString &priority) {
  if (priority == QLatin1String("critical")) return QColor(Qt::red);
  if (priority == QLatin1String("high")) return QColor(255, 165, 0);
  if (priority == QLatin1String("medium")) return QColor(Qt::yellow);
  // "low" or anything else: no special color.
  return {};
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

// Find or create the task_list sheet. Returns sheet_id.
static int ensureTaskSheet(ConfigManager *config) {
  // Look for an existing open sheet with role "task_list".
  QVector<ConfigManager::SheetData> sheets;
  QMetaObject::invokeMethod(config, [&] {
    sheets = config->LoadOpenSheets();
  }, Qt::BlockingQueuedConnection);
  for (const auto &s : sheets) {
    if (s.role == QLatin1String("task_list")) {
      return s.sheet_id;
    }
  }

  // Create a new task sheet.
  ConfigManager::SheetData sheet;
  sheet.name = QStringLiteral("Task Board");
  sheet.description = QStringLiteral("Agent task management board");
  sheet.role = QStringLiteral("task_list");

  for (const auto &col_name : kTaskColumnNames) {
    ConfigManager::SheetColumnInfo ci;
    ci.name = col_name;
    sheet.columns.append(ci);
  }

  int id = -1;
  QMetaObject::invokeMethod(config, [&] {
    id = config->SaveSheet(sheet);
  }, Qt::BlockingQueuedConnection);
  QMetaObject::invokeMethod(config,
      &ConfigManager::NotifyExternalSheetsChanged, Qt::QueuedConnection);
  return id;
}

// Find the row index for a task ID in the sheet. Returns -1 if not found.
static int findTaskRow(const ConfigManager::SheetData &sheet,
                       const QString &task_id) {
  for (int r = 0; r < sheet.cells.size(); ++r) {
    if (kColId < sheet.cells[r].size()) {
      QString id_text = cell_display_text(sheet.cells[r][kColId]);
      if (id_text == task_id) {
        return r;
      }
    }
  }
  return -1;
}

// Get the next task ID (reads existing IDs, returns "T-NNN").
static QString nextTaskId(const ConfigManager::SheetData &sheet) {
  int max_num = 0;
  for (int r = 0; r < sheet.cells.size(); ++r) {
    if (kColId < sheet.cells[r].size()) {
      QString id_text = cell_display_text(sheet.cells[r][kColId]);
      if (id_text.startsWith(QLatin1String("T-"))) {
        bool ok = false;
        int num = id_text.mid(2).toInt(&ok);
        if (ok && num > max_num) {
          max_num = num;
        }
      }
    }
  }
  return QStringLiteral("T-%1").arg(max_num + 1, 3, 10, QLatin1Char('0'));
}

}  // namespace

// ===========================================================================
// CreateTaskTool
// ===========================================================================

QString CreateTaskTool::name(void) const {
  return QStringLiteral("create_task");
}

QString CreateTaskTool::description(void) const {
  return QStringLiteral(
      "Create a new task on the task board with description, priority, "
      "and optional entity reference.");
}

QJsonObject CreateTaskTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("description")] = string_prop(
      QStringLiteral("Description of the task"));
  props[QStringLiteral("priority")] = string_prop(
      QStringLiteral("Priority: critical, high, medium (default), low"));
  props[QStringLiteral("entity")] = string_prop(
      QStringLiteral("Entity reference, e.g. \"func:12345\""));
  props[QStringLiteral("notes")] = string_prop(
      QStringLiteral("Additional notes"));
  return make_schema(props, {QStringLiteral("description")});
}

QJsonObject CreateTaskTool::execute(const QJsonObject &args) {
  if (!m_ctx->config) {
    return error_result(QStringLiteral("No config manager"));
  }

  QString desc = args[QStringLiteral("description")].toString();
  if (desc.isEmpty()) {
    return error_result(QStringLiteral("description is required"));
  }

  QString priority = args[QStringLiteral("priority")].toString();
  if (priority.isEmpty()) {
    priority = QStringLiteral("medium");
  }

  QString entity = args[QStringLiteral("entity")].toString();
  QString notes = args[QStringLiteral("notes")].toString();

  int sheet_id = ensureTaskSheet(m_ctx->config);
  if (sheet_id < 0) {
    return error_result(QStringLiteral("Failed to create/find task sheet"));
  }

  ConfigManager::SheetData sheet;
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    sheet = m_ctx->config->LoadSheetById(sheet_id);
  }, Qt::BlockingQueuedConnection);
  QString task_id = nextTaskId(sheet);
  int row = static_cast<int>(sheet.cells.size());

  // Ensure we have enough rows.
  ensure_cell(sheet, row, kNumCols - 1);

  sheet.cells[row][kColId] = make_string_cell(task_id);
  sheet.cells[row][kColDescription] = make_string_cell(desc);
  sheet.cells[row][kColStatus] = make_string_cell(QStringLiteral("planned"));
  sheet.cells[row][kColPriority] = make_string_cell(priority);
  sheet.cells[row][kColEntity] = make_string_cell(entity);
  sheet.cells[row][kColNotes] = make_string_cell(notes);
  sheet.cells[row][kColDone] = make_bool_cell(false);

  // Set row color by priority.
  QColor color = priority_color(priority);
  if (color.isValid()) {
    sheet.row_colors[row] = color;
  }

  QMetaObject::invokeMethod(m_ctx->config, [&] {
    m_ctx->config->SaveSheet(sheet);
  }, Qt::BlockingQueuedConnection);
  QMetaObject::invokeMethod(m_ctx->config,
      &ConfigManager::NotifyExternalSheetsChanged, Qt::QueuedConnection);

  QJsonObject result;
  result[QStringLiteral("task_id")] = task_id;
  result[QStringLiteral("sheet_id")] = sheet_id;
  result[QStringLiteral("row")] = row;
  return result;
}

// ===========================================================================
// UpdateTaskTool
// ===========================================================================

QString UpdateTaskTool::name(void) const {
  return QStringLiteral("update_task");
}

QString UpdateTaskTool::description(void) const {
  return QStringLiteral(
      "Update a task's status, priority, or notes by task ID.");
}

QJsonObject UpdateTaskTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("task_id")] = string_prop(
      QStringLiteral("Task ID, e.g. \"T-003\""));
  props[QStringLiteral("status")] = string_prop(
      QStringLiteral("New status: planned, in_progress, blocked, "
                     "completed, cancelled"));
  props[QStringLiteral("priority")] = string_prop(
      QStringLiteral("New priority: critical, high, medium, low"));
  props[QStringLiteral("notes")] = string_prop(
      QStringLiteral("Updated notes"));
  return make_schema(props, {QStringLiteral("task_id")});
}

QJsonObject UpdateTaskTool::execute(const QJsonObject &args) {
  if (!m_ctx->config) {
    return error_result(QStringLiteral("No config manager"));
  }

  QString task_id = args[QStringLiteral("task_id")].toString();
  if (task_id.isEmpty()) {
    return error_result(QStringLiteral("task_id is required"));
  }

  int sheet_id = ensureTaskSheet(m_ctx->config);
  if (sheet_id < 0) {
    return error_result(QStringLiteral("Failed to find task sheet"));
  }

  ConfigManager::SheetData sheet;
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    sheet = m_ctx->config->LoadSheetById(sheet_id);
  }, Qt::BlockingQueuedConnection);
  int row = findTaskRow(sheet, task_id);
  if (row < 0) {
    return error_result(
        QStringLiteral("Task not found: ") + task_id);
  }

  // Update status if provided.
  if (args.contains(QStringLiteral("status"))) {
    QString status = args[QStringLiteral("status")].toString();
    ensure_cell(sheet, row, kColStatus);
    sheet.cells[row][kColStatus] = make_string_cell(status);

    if (status == QLatin1String("completed")) {
      ensure_cell(sheet, row, kColDone);
      sheet.cells[row][kColDone] = make_bool_cell(true);
      sheet.row_colors[row] = QColor(Qt::green);
    }
  }

  // Update priority if provided.
  if (args.contains(QStringLiteral("priority"))) {
    QString priority = args[QStringLiteral("priority")].toString();
    ensure_cell(sheet, row, kColPriority);
    sheet.cells[row][kColPriority] = make_string_cell(priority);

    // Update row color unless status is completed (green takes precedence).
    QString current_status = cell_display_text(sheet.cells[row][kColStatus]);
    if (current_status != QLatin1String("completed")) {
      QColor color = priority_color(priority);
      if (color.isValid()) {
        sheet.row_colors[row] = color;
      } else {
        sheet.row_colors.remove(row);
      }
    }
  }

  // Update notes if provided.
  if (args.contains(QStringLiteral("notes"))) {
    ensure_cell(sheet, row, kColNotes);
    sheet.cells[row][kColNotes] =
        make_string_cell(args[QStringLiteral("notes")].toString());
  }

  QMetaObject::invokeMethod(m_ctx->config, [&] {
    m_ctx->config->SaveSheet(sheet);
  }, Qt::BlockingQueuedConnection);
  QMetaObject::invokeMethod(m_ctx->config,
      &ConfigManager::NotifyExternalSheetsChanged, Qt::QueuedConnection);

  QJsonObject result;
  result[QStringLiteral("success")] = true;
  result[QStringLiteral("task_id")] = task_id;
  return result;
}

// ===========================================================================
// ListTasksTool
// ===========================================================================

QString ListTasksTool::name(void) const {
  return QStringLiteral("list_tasks");
}

QString ListTasksTool::description(void) const {
  return QStringLiteral(
      "List tasks from the task board, optionally filtered by status "
      "or priority.");
}

QJsonObject ListTasksTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("status")] = string_prop(
      QStringLiteral("Filter by status"));
  props[QStringLiteral("priority")] = string_prop(
      QStringLiteral("Filter by priority"));
  return make_schema(props, {});
}

QJsonObject ListTasksTool::execute(const QJsonObject &args) {
  if (!m_ctx->config) {
    return error_result(QStringLiteral("No config manager"));
  }

  int sheet_id = ensureTaskSheet(m_ctx->config);
  if (sheet_id < 0) {
    return error_result(QStringLiteral("Failed to find task sheet"));
  }

  ConfigManager::SheetData sheet;
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    sheet = m_ctx->config->LoadSheetById(sheet_id);
  }, Qt::BlockingQueuedConnection);
  QString filter_status = args[QStringLiteral("status")].toString();
  QString filter_priority = args[QStringLiteral("priority")].toString();

  QJsonArray tasks;
  for (int r = 0; r < sheet.cells.size(); ++r) {
    if (sheet.cells[r].size() < kNumCols) continue;

    QString id_text = cell_display_text(sheet.cells[r][kColId]);
    if (id_text.isEmpty()) continue;

    QString status = cell_display_text(sheet.cells[r][kColStatus]);
    QString priority = cell_display_text(sheet.cells[r][kColPriority]);

    if (!filter_status.isEmpty() && status != filter_status) continue;
    if (!filter_priority.isEmpty() && priority != filter_priority) continue;

    QJsonObject task;
    task[QStringLiteral("task_id")] = id_text;
    task[QStringLiteral("description")] =
        cell_display_text(sheet.cells[r][kColDescription]);
    task[QStringLiteral("status")] = status;
    task[QStringLiteral("priority")] = priority;
    task[QStringLiteral("entity")] =
        cell_display_text(sheet.cells[r][kColEntity]);
    task[QStringLiteral("notes")] =
        cell_display_text(sheet.cells[r][kColNotes]);
    task[QStringLiteral("done")] =
        cell_display_text(sheet.cells[r][kColDone]) ==
        QLatin1String("true");
    tasks.append(task);
  }

  QJsonObject result;
  result[QStringLiteral("tasks")] = tasks;
  return result;
}

// ===========================================================================
// CompleteTaskTool
// ===========================================================================

QString CompleteTaskTool::name(void) const {
  return QStringLiteral("complete_task");
}

QString CompleteTaskTool::description(void) const {
  return QStringLiteral(
      "Mark a task as completed with optional completion notes.");
}

QJsonObject CompleteTaskTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("task_id")] = string_prop(
      QStringLiteral("Task ID, e.g. \"T-003\""));
  props[QStringLiteral("notes")] = string_prop(
      QStringLiteral("Completion notes"));
  return make_schema(props, {QStringLiteral("task_id")});
}

QJsonObject CompleteTaskTool::execute(const QJsonObject &args) {
  if (!m_ctx->config) {
    return error_result(QStringLiteral("No config manager"));
  }

  QString task_id = args[QStringLiteral("task_id")].toString();
  if (task_id.isEmpty()) {
    return error_result(QStringLiteral("task_id is required"));
  }

  int sheet_id = ensureTaskSheet(m_ctx->config);
  if (sheet_id < 0) {
    return error_result(QStringLiteral("Failed to find task sheet"));
  }

  ConfigManager::SheetData sheet;
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    sheet = m_ctx->config->LoadSheetById(sheet_id);
  }, Qt::BlockingQueuedConnection);
  int row = findTaskRow(sheet, task_id);
  if (row < 0) {
    return error_result(
        QStringLiteral("Task not found: ") + task_id);
  }

  ensure_cell(sheet, row, kColDone);
  sheet.cells[row][kColStatus] = make_string_cell(QStringLiteral("completed"));
  sheet.cells[row][kColDone] = make_bool_cell(true);
  sheet.row_colors[row] = QColor(Qt::green);

  // Append notes if provided.
  if (args.contains(QStringLiteral("notes"))) {
    QString new_notes = args[QStringLiteral("notes")].toString();
    QString existing = cell_display_text(sheet.cells[row][kColNotes]);
    if (!existing.isEmpty() && !new_notes.isEmpty()) {
      new_notes = existing + QStringLiteral("\n") + new_notes;
    } else if (new_notes.isEmpty()) {
      new_notes = existing;
    }
    sheet.cells[row][kColNotes] = make_string_cell(new_notes);
  }

  QMetaObject::invokeMethod(m_ctx->config, [&] {
    m_ctx->config->SaveSheet(sheet);
  }, Qt::BlockingQueuedConnection);
  QMetaObject::invokeMethod(m_ctx->config,
      &ConfigManager::NotifyExternalSheetsChanged, Qt::QueuedConnection);

  QJsonObject result;
  result[QStringLiteral("success")] = true;
  result[QStringLiteral("task_id")] = task_id;
  return result;
}

// ===========================================================================
// GetTaskBoardSummaryTool
// ===========================================================================

QString GetTaskBoardSummaryTool::name(void) const {
  return QStringLiteral("get_task_board_summary");
}

QString GetTaskBoardSummaryTool::description(void) const {
  return QStringLiteral(
      "Get a summary overview of the task board: counts by status "
      "and priority.");
}

QJsonObject GetTaskBoardSummaryTool::parametersSchema(void) const {
  return make_schema({}, {});
}

QJsonObject GetTaskBoardSummaryTool::execute(const QJsonObject &args) {
  Q_UNUSED(args);

  if (!m_ctx->config) {
    return error_result(QStringLiteral("No config manager"));
  }

  int sheet_id = ensureTaskSheet(m_ctx->config);
  if (sheet_id < 0) {
    return error_result(QStringLiteral("Failed to find task sheet"));
  }

  ConfigManager::SheetData sheet;
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    sheet = m_ctx->config->LoadSheetById(sheet_id);
  }, Qt::BlockingQueuedConnection);

  int total = 0;
  int planned = 0;
  int in_progress = 0;
  int blocked = 0;
  int completed = 0;
  int cancelled = 0;

  int p_critical = 0;
  int p_high = 0;
  int p_medium = 0;
  int p_low = 0;

  for (int r = 0; r < sheet.cells.size(); ++r) {
    if (sheet.cells[r].size() < kNumCols) continue;
    QString id_text = cell_display_text(sheet.cells[r][kColId]);
    if (id_text.isEmpty()) continue;

    ++total;

    QString status = cell_display_text(sheet.cells[r][kColStatus]);
    if (status == QLatin1String("planned")) ++planned;
    else if (status == QLatin1String("in_progress")) ++in_progress;
    else if (status == QLatin1String("blocked")) ++blocked;
    else if (status == QLatin1String("completed")) ++completed;
    else if (status == QLatin1String("cancelled")) ++cancelled;

    QString priority = cell_display_text(sheet.cells[r][kColPriority]);
    if (priority == QLatin1String("critical")) ++p_critical;
    else if (priority == QLatin1String("high")) ++p_high;
    else if (priority == QLatin1String("medium")) ++p_medium;
    else if (priority == QLatin1String("low")) ++p_low;
  }

  QJsonObject by_priority;
  by_priority[QStringLiteral("critical")] = p_critical;
  by_priority[QStringLiteral("high")] = p_high;
  by_priority[QStringLiteral("medium")] = p_medium;
  by_priority[QStringLiteral("low")] = p_low;

  QJsonObject result;
  result[QStringLiteral("total")] = total;
  result[QStringLiteral("planned")] = planned;
  result[QStringLiteral("in_progress")] = in_progress;
  result[QStringLiteral("blocked")] = blocked;
  result[QStringLiteral("completed")] = completed;
  result[QStringLiteral("cancelled")] = cancelled;
  result[QStringLiteral("by_priority")] = by_priority;
  return result;
}

// ===========================================================================
// Registration
// ===========================================================================

void registerTaskTools(AgentToolRegistry &registry, TaskToolContext *ctx) {
  registry.registerTool(std::make_unique<CreateTaskTool>(ctx));
  registry.registerTool(std::make_unique<UpdateTaskTool>(ctx));
  registry.registerTool(std::make_unique<ListTasksTool>(ctx));
  registry.registerTool(std::make_unique<CompleteTaskTool>(ctx));
  registry.registerTool(std::make_unique<GetTaskBoardSummaryTool>(ctx));
}

}  // namespace mx::gui
