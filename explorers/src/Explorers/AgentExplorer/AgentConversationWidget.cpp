// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "AgentConversationWidget.h"

#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMap>
#include <QMenu>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QTableWidget>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextEdit>
#include <QTreeWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

#include <multiplier/Frontend/TokenKind.h>
#include <multiplier/GUI/Interfaces/ITheme.h>
#include <multiplier/GUI/Managers/AgentMessage.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/ThemeManager.h>
#include <multiplier/GUI/Util.h>
#include <multiplier/Index.h>

namespace {

// Resolve an entity ID from JSON into a human-readable name.
// JSON uses double-precision floats, which cannot exactly represent all
// uint64_t values.  Fall back to hex when no name is available.
static QString resolveEntityName(const mx::gui::ConfigManager &config,
                                 const QJsonValue &id_val) {
  auto raw_id = static_cast<mx::RawEntityId>(id_val.toDouble(0));
  if (raw_id == 0 || raw_id == mx::kInvalidEntityId) {
    return QStringLiteral("(unknown)");
  }

  const auto &index = config.Index();
  auto entity = index.entity(mx::EntityId(raw_id));

  if (auto name = mx::gui::NameOfEntityAsString(entity)) {
    return name.value();
  }

  return QStringLiteral("0x%1").arg(raw_id, 0, 16);
}

static QString toolResultSummary(const QString &tool_name,
                                 const QJsonObject &result) {
  if (result.contains(QStringLiteral("error"))) {
    return QStringLiteral("Error: %1").arg(
        result[QStringLiteral("error")].toString());
  }

  if (tool_name == QStringLiteral("list_files")) {
    int count = result[QStringLiteral("count")].toInt();
    return QStringLiteral("Files: %1 found").arg(count);
  }
  if (tool_name == QStringLiteral("search_entities")) {
    int count = result[QStringLiteral("count")].toInt();
    return QStringLiteral("Entities: %1 found").arg(count);
  }
  if (tool_name == QStringLiteral("search_code")) {
    int count = result[QStringLiteral("count")].toInt();
    return QStringLiteral("Code matches: %1 found").arg(count);
  }
  if (tool_name == QStringLiteral("get_references")) {
    int count = result[QStringLiteral("count")].toInt();
    bool has_more = result[QStringLiteral("has_more")].toBool();
    return QStringLiteral("References: %1 found%2")
        .arg(count)
        .arg(has_more ? QStringLiteral(" (has_more: true)") : QString());
  }
  if (tool_name == QStringLiteral("get_callers")) {
    int count = result[QStringLiteral("count")].toInt();
    return QStringLiteral("Callers: %1 found").arg(count);
  }
  if (tool_name == QStringLiteral("get_callees")) {
    int count = result[QStringLiteral("count")].toInt();
    return QStringLiteral("Callees: %1 found").arg(count);
  }
  if (tool_name == QStringLiteral("get_definition")) {
    auto file = result[QStringLiteral("file")].toString();
    int line = result[QStringLiteral("line")].toInt(-1);
    if (!file.isEmpty() && line >= 0) {
      // Show just the filename, not the full path.
      auto sep = file.lastIndexOf(QLatin1Char('/'));
      auto short_name = (sep >= 0) ? file.mid(sep + 1) : file;
      return QStringLiteral("Definition: %1:%2").arg(short_name).arg(line);
    }
    if (!file.isEmpty()) {
      auto sep = file.lastIndexOf(QLatin1Char('/'));
      return QStringLiteral("Definition: %1")
          .arg((sep >= 0) ? file.mid(sep + 1) : file);
    }
    return QStringLiteral("Definition retrieved");
  }
  if (tool_name == QStringLiteral("create_task")) {
    auto task_id = result[QStringLiteral("task_id")].toString();
    return QStringLiteral("Task %1 created").arg(task_id);
  }
  if (tool_name == QStringLiteral("create_sheet")) {
    int id = result[QStringLiteral("sheet_id")].toInt();
    return QStringLiteral("Sheet created (id: %1)").arg(id);
  }
  if (tool_name == QStringLiteral("create_document")) {
    auto title = result[QStringLiteral("title")].toString();
    return QStringLiteral("Document '%1' created").arg(title);
  }
  if (tool_name == QStringLiteral("create_findings_sheet")) {
    int id = result[QStringLiteral("sheet_id")].toInt();
    return QStringLiteral("Findings sheet created (id: %1)").arg(id);
  }
  if (tool_name == QStringLiteral("create_attack_surface_sheet")) {
    int id = result[QStringLiteral("sheet_id")].toInt();
    return QStringLiteral("Attack surface sheet created (id: %1)").arg(id);
  }
  if (tool_name == QStringLiteral("link_document_to_cell")) {
    return QStringLiteral("Document linked to cell");
  }
  if (tool_name == QStringLiteral("get_session_cost")) {
    double cost = result[QStringLiteral("total_cost_usd")].toDouble();
    int llm_calls = result[QStringLiteral("llm_calls")].toInt();
    int tool_calls = result[QStringLiteral("tool_calls")].toInt();
    return QStringLiteral("Cost: $%1 (%2 LLM calls, %3 tool calls)")
        .arg(cost, 0, 'f', 2)
        .arg(llm_calls)
        .arg(tool_calls);
  }
  if (tool_name == QStringLiteral("finish")) {
    return QStringLiteral("Finished: acknowledged");
  }
  if (tool_name == QStringLiteral("get_task_board_summary")) {
    int total = result[QStringLiteral("total")].toInt();
    return QStringLiteral("Task board: %1 tasks").arg(total);
  }
  return QStringLiteral("Result: %1").arg(tool_name);
}

static void formatCallersTree(const QJsonArray &items, const QString &prefix,
                              QStringList &lines, int depth) {
  for (qsizetype i = 0; i < items.size(); ++i) {
    auto obj = items[i].toObject();
    auto name = obj[QStringLiteral("name")].toString();
    auto kind = obj[QStringLiteral("kind")].toString();
    auto file = obj[QStringLiteral("file")].toString();
    int line = obj[QStringLiteral("line")].toInt(-1);
    bool is_last = (i == items.size() - 1);
    auto connector = is_last ? QStringLiteral("\xe2\x94\x94\xe2\x94\x80 ")
                             : QStringLiteral("\xe2\x94\x9c\xe2\x94\x80 ");

    QString loc;
    if (!file.isEmpty()) {
      auto sep = file.lastIndexOf(QLatin1Char('/'));
      auto short_file = (sep >= 0) ? file.mid(sep + 1) : file;
      loc = (line >= 0) ? QStringLiteral(" %1:%2").arg(short_file).arg(line)
                        : QStringLiteral(" %1").arg(short_file);
    }

    QString entry;
    if (depth == 0) {
      entry = QStringLiteral("%1 (%2)%3")
                  .arg(name, kind)
                  .arg(loc);
    } else {
      entry = QStringLiteral("%1%2%3 (%4)%5")
                  .arg(prefix, connector, name, kind)
                  .arg(loc);
    }
    lines.append(entry);

    // Recurse into sub-items (callers or callees).
    auto child_prefix =
        prefix + (is_last ? QStringLiteral("    ") : QStringLiteral("\xe2\x94\x82   "));
    auto sub_key = obj.contains(QStringLiteral("callers"))
                       ? QStringLiteral("callers")
                       : QStringLiteral("callees");
    if (obj.contains(sub_key)) {
      formatCallersTree(obj[sub_key].toArray(), child_prefix, lines,
                        depth + 1);
    }
  }
}

static QString formatToolResult(const QString &tool_name,
                                const QJsonObject &result) {
  if (result.contains(QStringLiteral("error"))) {
    return QStringLiteral("Error: %1")
        .arg(result[QStringLiteral("error")].toString());
  }

  if (tool_name == QStringLiteral("list_files")) {
    auto files = result[QStringLiteral("files")].toArray();
    QStringList lines;
    auto limit = qMin(files.size(), qsizetype(20));
    for (qsizetype i = 0; i < limit; ++i) {
      auto obj = files[i].toObject();
      lines.append(obj[QStringLiteral("path")].toString());
    }
    if (files.size() > 20) {
      lines.append(
          QStringLiteral("...and %1 more").arg(files.size() - 20));
    }
    return lines.join(QLatin1Char('\n'));
  }

  if (tool_name == QStringLiteral("search_entities")) {
    auto entities = result[QStringLiteral("entities")].toArray();
    QStringList lines;
    lines.append(QStringLiteral("kind     | name"));
    for (const auto &val : entities) {
      auto obj = val.toObject();
      auto kind = obj[QStringLiteral("kind")].toString();
      auto name = obj[QStringLiteral("name")].toString();
      lines.append(QStringLiteral("%1 | %2")
                       .arg(kind, -8)
                       .arg(name));
    }
    return lines.join(QLatin1Char('\n'));
  }

  if (tool_name == QStringLiteral("search_code")) {
    auto matches = result[QStringLiteral("matches")].toArray();
    QStringList lines;
    for (const auto &val : matches) {
      auto obj = val.toObject();
      auto file = obj[QStringLiteral("file")].toString();
      int line_no = obj[QStringLiteral("line")].toInt(-1);
      auto context = obj[QStringLiteral("context")].toString().trimmed();
      // Show just the filename.
      auto sep = file.lastIndexOf(QLatin1Char('/'));
      auto short_file = (sep >= 0) ? file.mid(sep + 1) : file;
      if (line_no >= 0) {
        lines.append(QStringLiteral("%1:%2: %3")
                         .arg(short_file)
                         .arg(line_no)
                         .arg(context));
      } else {
        lines.append(QStringLiteral("%1: %2").arg(short_file, context));
      }
    }
    return lines.join(QLatin1Char('\n'));
  }

  if (tool_name == QStringLiteral("get_references")) {
    auto refs = result[QStringLiteral("references")].toArray();
    QStringList lines;
    for (const auto &val : refs) {
      auto obj = val.toObject();
      auto ref_kind = obj[QStringLiteral("ref_kind")].toString();
      auto file = obj[QStringLiteral("file")].toString();
      int line_no = obj[QStringLiteral("line")].toInt(-1);
      auto context = obj[QStringLiteral("context")].toString().trimmed();
      auto sep = file.lastIndexOf(QLatin1Char('/'));
      auto short_file = (sep >= 0) ? file.mid(sep + 1) : file;
      QString loc = (line_no >= 0)
                        ? QStringLiteral("%1:%2").arg(short_file).arg(line_no)
                        : short_file;
      lines.append(
          QStringLiteral("[%1] %2 \xe2\x80\x94 %3").arg(ref_kind, loc, context));
    }
    bool has_more = result[QStringLiteral("has_more")].toBool();
    if (has_more) {
      int total = result[QStringLiteral("total_matched")].toInt();
      lines.append(QStringLiteral("... %1 total matches").arg(total));
    }
    return lines.join(QLatin1Char('\n'));
  }

  if (tool_name == QStringLiteral("get_callers") ||
      tool_name == QStringLiteral("get_callees")) {
    auto key = (tool_name == QStringLiteral("get_callers"))
                   ? QStringLiteral("callers")
                   : QStringLiteral("callees");
    auto items = result[key].toArray();
    QStringList lines;
    formatCallersTree(items, QString(), lines, 0);
    return lines.join(QLatin1Char('\n'));
  }

  if (tool_name == QStringLiteral("get_definition")) {
    auto code = result[QStringLiteral("code")].toString();
    auto file = result[QStringLiteral("file")].toString();
    int line = result[QStringLiteral("line")].toInt(-1);
    QString header;
    if (!file.isEmpty()) {
      auto sep = file.lastIndexOf(QLatin1Char('/'));
      auto short_file = (sep >= 0) ? file.mid(sep + 1) : file;
      header = (line >= 0) ? QStringLiteral("// %1:%2\n").arg(short_file).arg(line)
                           : QStringLiteral("// %1\n").arg(short_file);
    }
    return header + code;
  }

  if (tool_name == QStringLiteral("get_task_board_summary")) {
    int total = result[QStringLiteral("total")].toInt();
    int planned = result[QStringLiteral("planned")].toInt();
    int in_progress = result[QStringLiteral("in_progress")].toInt();
    int blocked = result[QStringLiteral("blocked")].toInt();
    int completed = result[QStringLiteral("completed")].toInt();
    int cancelled = result[QStringLiteral("cancelled")].toInt();

    QStringList lines;
    lines.append(QStringLiteral("Tasks: %1 total | %2 planned | "
                                "%3 in_progress | %4 blocked | "
                                "%5 completed | %6 cancelled")
                     .arg(total)
                     .arg(planned)
                     .arg(in_progress)
                     .arg(blocked)
                     .arg(completed)
                     .arg(cancelled));

    auto by_priority = result[QStringLiteral("by_priority")].toObject();
    if (!by_priority.isEmpty()) {
      QStringList priority_parts;
      for (auto it = by_priority.begin(); it != by_priority.end(); ++it) {
        priority_parts.append(
            QStringLiteral("%1 %2").arg(it.value().toInt()).arg(it.key()));
      }
      lines.append(
          QStringLiteral("By priority: %1").arg(priority_parts.join(QStringLiteral(" | "))));
    }
    return lines.join(QLatin1Char('\n'));
  }

  if (tool_name == QStringLiteral("get_session_cost")) {
    double cost = result[QStringLiteral("total_cost_usd")].toDouble();
    int in_tok = result[QStringLiteral("total_input_tokens")].toInt();
    int out_tok = result[QStringLiteral("total_output_tokens")].toInt();
    int llm_calls = result[QStringLiteral("llm_calls")].toInt();
    int tool_calls = result[QStringLiteral("tool_calls")].toInt();

    QStringList lines;
    lines.append(QStringLiteral("Total cost: $%1").arg(cost, 0, 'f', 2));
    lines.append(QStringLiteral("LLM calls: %1 | Tool calls: %2")
                     .arg(llm_calls)
                     .arg(tool_calls));
    lines.append(QStringLiteral("Tokens: %L1 input | %L2 output")
                     .arg(in_tok)
                     .arg(out_tok));

    auto by_tool = result[QStringLiteral("by_tool")].toArray();
    if (!by_tool.isEmpty()) {
      lines.append(QString());
      lines.append(QStringLiteral("By tool:"));
      for (const auto &val : by_tool) {
        auto obj = val.toObject();
        lines.append(QStringLiteral("  %1: %2 calls, $%3")
                         .arg(obj[QStringLiteral("tool")].toString())
                         .arg(obj[QStringLiteral("calls")].toInt())
                         .arg(obj[QStringLiteral("cost_usd")].toDouble(),
                              0, 'f', 3));
      }
    }
    return lines.join(QLatin1Char('\n'));
  }

  // Default: pretty-print JSON.
  return QString::fromUtf8(
      QJsonDocument(result).toJson(QJsonDocument::Indented));
}

static QString formatToolArgs(const QString &tool_name,
                              const QJsonObject &args,
                              const mx::gui::ConfigManager &config) {
  if (args.isEmpty()) {
    return QStringLiteral("(no args)");
  }

  if (tool_name == QStringLiteral("list_files")) {
    return QStringLiteral("(no args)");
  }
  if (tool_name == QStringLiteral("run_python") ||
      tool_name == QStringLiteral("create_script_file")) {
    auto code = args[QStringLiteral("code")].toString();
    if (!code.isEmpty()) {
      return code;
    }
  }
  if (tool_name == QStringLiteral("search_entities")) {
    QStringList parts;
    auto query = args[QStringLiteral("query")].toString();
    if (!query.isEmpty()) {
      parts.append(QStringLiteral("query: '%1'").arg(query));
    }
    auto kind = args[QStringLiteral("kind")].toString();
    if (!kind.isEmpty()) {
      parts.append(QStringLiteral("kind: '%1'").arg(kind));
    }
    if (!parts.isEmpty()) {
      return parts.join(QStringLiteral(", "));
    }
  }
  if (tool_name == QStringLiteral("search_code")) {
    auto pattern = args[QStringLiteral("pattern")].toString();
    if (!pattern.isEmpty()) {
      return QStringLiteral("pattern: '%1'").arg(pattern);
    }
  }
  if (tool_name == QStringLiteral("get_definition") ||
      tool_name == QStringLiteral("get_references") ||
      tool_name == QStringLiteral("get_callers") ||
      tool_name == QStringLiteral("get_callees")) {
    auto id_val = args[QStringLiteral("entity_id")];
    auto raw_id = static_cast<mx::RawEntityId>(id_val.toDouble(0));
    if (raw_id != 0) {
      auto name = resolveEntityName(config, id_val);
      return QStringLiteral("entity: %1").arg(name);
    }
  }
  if (tool_name == QStringLiteral("finish")) {
    auto summary = args[QStringLiteral("summary")].toString();
    auto status = args[QStringLiteral("status")].toString(
        QStringLiteral("completed"));
    return QStringLiteral("status: %1, summary: '%2'")
        .arg(status, summary.left(80));
  }
  if (tool_name == QStringLiteral("create_task")) {
    auto desc = args[QStringLiteral("description")].toString();
    return QStringLiteral("'%1'").arg(desc.left(60));
  }
  if (tool_name == QStringLiteral("create_sheet")) {
    auto name = args[QStringLiteral("name")].toString();
    return QStringLiteral("name: '%1'").arg(name);
  }
  if (tool_name == QStringLiteral("create_document")) {
    auto title = args[QStringLiteral("title")].toString();
    return QStringLiteral("title: '%1'").arg(title);
  }

  // Default: compact key=value pairs.
  QStringList parts;
  for (auto it = args.begin(); it != args.end(); ++it) {
    auto val = it.value();
    if (val.isString()) {
      auto s = val.toString();
      if (s.size() > 40) {
        s = s.left(37) + QStringLiteral("...");
      }
      parts.append(QStringLiteral("%1: '%2'").arg(it.key(), s));
    } else if (val.isDouble()) {
      parts.append(QStringLiteral("%1: %2")
                       .arg(it.key())
                       .arg(val.toDouble()));
    } else if (val.isBool()) {
      parts.append(QStringLiteral("%1: %2")
                       .arg(it.key(),
                            val.toBool() ? QStringLiteral("true")
                                         : QStringLiteral("false")));
    }
  }
  if (parts.isEmpty()) {
    return QString::fromUtf8(
        QJsonDocument(args).toJson(QJsonDocument::Compact));
  }
  return parts.join(QStringLiteral(", "));
}

// Custom data role for storing entity IDs on list/table items.
static constexpr int kEntityIdRole = Qt::UserRole + 10;

// Helper: apply compact styling to a list/table/tree widget.
static void applyCompactStyle(QAbstractItemView *view, int max_height) {
  auto f = view->font();
  f.setPointSize(f.pointSize() - 1);
  view->setFont(f);
  view->setMaximumHeight(max_height);
  view->setEditTriggers(QAbstractItemView::NoEditTriggers);
  view->setSelectionMode(QAbstractItemView::SingleSelection);
  view->setFrameShape(QFrame::NoFrame);
  view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

// 1. list_files -> clickable file list
static QWidget *createListFilesView(const QJsonObject &result,
                                    const mx::gui::ConfigManager &config,
                                    QWidget *parent) {
  auto files = result[QStringLiteral("files")].toArray();
  if (files.isEmpty()) {
    return nullptr;
  }

  auto *list = new QListWidget(parent);
  applyCompactStyle(list, 200);
  list->setSpacing(0);

  auto limit = qMin(files.size(), qsizetype(30));
  for (qsizetype i = 0; i < limit; ++i) {
    auto obj = files[i].toObject();
    auto path = obj[QStringLiteral("path")].toString();
    auto raw_id = static_cast<mx::RawEntityId>(
        obj[QStringLiteral("entity_id")].toDouble(0));
    auto *item = new QListWidgetItem(path, list);
    item->setData(Qt::UserRole, qlonglong(raw_id));
    auto resolved = resolveEntityName(config,
                                      obj[QStringLiteral("entity_id")]);
    item->setToolTip(resolved);
  }
  if (files.size() > 30) {
    auto *more = new QListWidgetItem(
        QStringLiteral("...and %1 more").arg(files.size() - 30), list);
    more->setFlags(more->flags() & ~Qt::ItemIsSelectable);
    auto f = more->font();
    f.setItalic(true);
    more->setFont(f);
  }
  return list;
}

// 2. search_entities -> mini table
static QWidget *createSearchEntitiesView(const QJsonObject &result,
                                         const mx::gui::ConfigManager &config,
                                         QWidget *parent) {
  auto entities = result[QStringLiteral("entities")].toArray();
  if (entities.isEmpty()) {
    return nullptr;
  }

  auto *table = new QTableWidget(static_cast<int>(entities.size()), 2, parent);
  table->setHorizontalHeaderLabels({QStringLiteral("Kind"),
                                    QStringLiteral("Name")});
  applyCompactStyle(table, 250);
  table->setAlternatingRowColors(true);
  table->verticalHeader()->setVisible(false);
  table->verticalHeader()->setDefaultSectionSize(20);
  table->horizontalHeader()->setStretchLastSection(true);

  for (qsizetype i = 0; i < entities.size(); ++i) {
    auto obj = entities[i].toObject();
    auto kind = obj[QStringLiteral("kind")].toString();
    auto name = obj[QStringLiteral("name")].toString();

    // If the JSON name is empty, try to resolve from the index.
    if (name.isEmpty()) {
      name = resolveEntityName(config, obj[QStringLiteral("entity_id")]);
    }

    auto eid_val = obj[QStringLiteral("entity_id")];
    quint64 eid = eid_val.isString()
        ? eid_val.toString().toULongLong()
        : static_cast<quint64>(eid_val.toDouble(0));

    auto row = static_cast<int>(i);
    auto *kind_item = new QTableWidgetItem(kind);
    kind_item->setData(kEntityIdRole, QVariant::fromValue(eid));
    kind_item->setToolTip(QStringLiteral("Double-click to open"));
    table->setItem(row, 0, kind_item);

    auto *name_item = new QTableWidgetItem(name);
    name_item->setData(kEntityIdRole, QVariant::fromValue(eid));
    name_item->setToolTip(QStringLiteral("Double-click to open"));
    table->setItem(row, 1, name_item);
  }
  table->resizeColumnsToContents();
  return table;
}

// 3. search_code -> mini grep results
static QWidget *createSearchCodeView(const QJsonObject &result,
                                     QWidget *parent) {
  auto matches = result[QStringLiteral("matches")].toArray();
  if (matches.isEmpty()) {
    return nullptr;
  }

  auto *list = new QListWidget(parent);
  applyCompactStyle(list, 250);

  for (const auto &val : matches) {
    auto obj = val.toObject();
    auto file = obj[QStringLiteral("file")].toString();
    int line_no = obj[QStringLiteral("line")].toInt(-1);
    auto context = obj[QStringLiteral("context")].toString().trimmed();
    auto sep = file.lastIndexOf(QLatin1Char('/'));
    auto short_file = (sep >= 0) ? file.mid(sep + 1) : file;

    auto eid_val = obj[QStringLiteral("entity_id")];
    quint64 eid = eid_val.isString()
        ? eid_val.toString().toULongLong()
        : static_cast<quint64>(eid_val.toDouble(0));

    QString text;
    if (line_no >= 0) {
      text = QStringLiteral("%1:%2: %3")
                 .arg(short_file).arg(line_no).arg(context);
    } else {
      text = QStringLiteral("%1: %2").arg(short_file, context);
    }
    auto *item = new QListWidgetItem(text, list);
    auto f = item->font();
    f.setFamily(QStringLiteral("monospace"));
    item->setFont(f);
    item->setData(kEntityIdRole, QVariant::fromValue(eid));
    item->setToolTip(QStringLiteral("Double-click to open"));
  }
  return list;
}

// 4. get_references -> color-coded reference list
static QWidget *createReferencesView(const QJsonObject &result,
                                     QWidget *parent) {
  auto refs = result[QStringLiteral("references")].toArray();
  if (refs.isEmpty()) {
    return nullptr;
  }

  auto *list = new QListWidget(parent);
  applyCompactStyle(list, 250);

  for (const auto &val : refs) {
    auto obj = val.toObject();
    auto ref_kind = obj[QStringLiteral("ref_kind")].toString();
    auto file = obj[QStringLiteral("file")].toString();
    int line_no = obj[QStringLiteral("line")].toInt(-1);
    auto context = obj[QStringLiteral("context")].toString().trimmed();
    auto sep = file.lastIndexOf(QLatin1Char('/'));
    auto short_file = (sep >= 0) ? file.mid(sep + 1) : file;
    QString loc = (line_no >= 0)
                      ? QStringLiteral("%1:%2").arg(short_file).arg(line_no)
                      : short_file;

    auto text = QStringLiteral("[%1] %2 \xe2\x80\x94 %3")
                    .arg(ref_kind, loc, context);
    auto *item = new QListWidgetItem(text, list);

    // Color-code by reference kind.
    QColor color;
    if (ref_kind == QStringLiteral("CALLS") ||
        ref_kind == QStringLiteral("CALL")) {
      color = QColor(0x3B, 0x82, 0xF6);  // blue
    } else if (ref_kind == QStringLiteral("WRITES") ||
               ref_kind == QStringLiteral("WRITE")) {
      color = QColor(0xF9, 0x73, 0x16);  // orange
    } else if (ref_kind == QStringLiteral("READS") ||
               ref_kind == QStringLiteral("READ")) {
      color = QColor(0x22, 0xC5, 0x5E);  // green
    } else if (ref_kind == QStringLiteral("TAKES_ADDRESS") ||
               ref_kind == QStringLiteral("ADDRESS_OF")) {
      color = QColor(0xA8, 0x55, 0xF7);  // purple
    } else {
      color = QColor(0x9C, 0xA3, 0xAF);  // gray
    }
    item->setForeground(color);
  }

  bool has_more = result[QStringLiteral("has_more")].toBool();
  if (has_more) {
    int total = result[QStringLiteral("total_matched")].toInt();
    auto *more = new QListWidgetItem(
        QStringLiteral("... %1 total matches").arg(total), list);
    more->setFlags(more->flags() & ~Qt::ItemIsSelectable);
    auto f = more->font();
    f.setItalic(true);
    more->setFont(f);
  }
  return list;
}

// Helper: recursively populate a QTreeWidgetItem from caller/callee data.
static void populateCallTree(QTreeWidgetItem *parent_item,
                             const QJsonArray &items,
                             const QString &child_key) {
  for (const auto &val : items) {
    auto obj = val.toObject();
    auto name = obj[QStringLiteral("name")].toString();
    auto kind = obj[QStringLiteral("kind")].toString();
    auto file = obj[QStringLiteral("file")].toString();
    int line = obj[QStringLiteral("line")].toInt(-1);

    auto sep = file.lastIndexOf(QLatin1Char('/'));
    auto short_file = (sep >= 0) ? file.mid(sep + 1) : file;
    QString loc;
    if (!short_file.isEmpty()) {
      loc = (line >= 0) ? QStringLiteral("%1:%2").arg(short_file).arg(line)
                        : short_file;
    }

    auto display = QStringLiteral("%1 (%2)").arg(name, kind);
    auto *child = new QTreeWidgetItem(parent_item, {display, loc});

    if (obj.contains(child_key)) {
      populateCallTree(child, obj[child_key].toArray(), child_key);
    }
  }
}

// 5. get_callers / get_callees -> mini tree
static QWidget *createCallTreeView(const QJsonObject &result,
                                   const QString &tool_name,
                                   QWidget *parent) {
  auto key = (tool_name == QStringLiteral("get_callers"))
                 ? QStringLiteral("callers")
                 : QStringLiteral("callees");
  auto items = result[key].toArray();
  if (items.isEmpty()) {
    return nullptr;
  }

  auto *tree = new QTreeWidget(parent);
  tree->setHeaderLabels({QStringLiteral("Name"), QStringLiteral("File:Line")});
  applyCompactStyle(tree, 250);
  tree->header()->setStretchLastSection(true);

  for (const auto &val : items) {
    auto obj = val.toObject();
    auto name = obj[QStringLiteral("name")].toString();
    auto kind = obj[QStringLiteral("kind")].toString();
    auto file = obj[QStringLiteral("file")].toString();
    int line = obj[QStringLiteral("line")].toInt(-1);

    auto sep = file.lastIndexOf(QLatin1Char('/'));
    auto short_file = (sep >= 0) ? file.mid(sep + 1) : file;
    QString loc;
    if (!short_file.isEmpty()) {
      loc = (line >= 0) ? QStringLiteral("%1:%2").arg(short_file).arg(line)
                        : short_file;
    }

    auto display = QStringLiteral("%1 (%2)").arg(name, kind);
    auto *root = new QTreeWidgetItem(tree, {display, loc});

    if (obj.contains(key)) {
      populateCallTree(root, obj[key].toArray(), key);
    }
  }
  tree->expandAll();
  tree->resizeColumnToContents(0);
  return tree;
}

// 6. get_definition -> mini code view
static QWidget *createDefinitionView(const QJsonObject &result,
                                     QWidget *parent) {
  auto code = result[QStringLiteral("code")].toString();
  if (code.isEmpty()) {
    return nullptr;
  }

  auto file = result[QStringLiteral("file")].toString();
  int line = result[QStringLiteral("line")].toInt(-1);

  QString header;
  if (!file.isEmpty()) {
    auto sep = file.lastIndexOf(QLatin1Char('/'));
    auto short_file = (sep >= 0) ? file.mid(sep + 1) : file;
    header = (line >= 0)
                 ? QStringLiteral("// %1:%2\n").arg(short_file).arg(line)
                 : QStringLiteral("// %1\n").arg(short_file);
  }

  auto *edit = new QPlainTextEdit(parent);
  edit->setReadOnly(true);
  edit->setPlainText(header + code);
  edit->setMaximumHeight(300);
  edit->setFrameShape(QFrame::NoFrame);

  auto f = edit->font();
  f.setFamily(QStringLiteral("monospace"));
  f.setPointSize(f.pointSize() - 1);
  edit->setFont(f);
  edit->setLineWrapMode(QPlainTextEdit::NoWrap);
  return edit;
}

// 7. get_task_board_summary -> colored pill dashboard
static QWidget *createTaskBoardView(const QJsonObject &result,
                                    QWidget *parent) {
  int total = result[QStringLiteral("total")].toInt();
  if (total == 0) {
    return nullptr;
  }

  auto *widget = new QWidget(parent);
  auto *layout = new QHBoxLayout(widget);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);

  struct PillInfo {
    QString key;
    QString label;
    QColor color;
  };

  PillInfo pills[] = {
    {QStringLiteral("planned"), QStringLiteral("planned"),
     QColor(0x9C, 0xA3, 0xAF)},
    {QStringLiteral("in_progress"), QStringLiteral("in_progress"),
     QColor(0x3B, 0x82, 0xF6)},
    {QStringLiteral("blocked"), QStringLiteral("blocked"),
     QColor(0xEF, 0x44, 0x44)},
    {QStringLiteral("completed"), QStringLiteral("completed"),
     QColor(0x22, 0xC5, 0x5E)},
    {QStringLiteral("cancelled"), QStringLiteral("cancelled"),
     QColor(0x78, 0x71, 0x6C)},
  };

  for (const auto &p : pills) {
    int count = result[p.key].toInt();
    if (count == 0) {
      continue;
    }
    auto *pill = new QLabel(
        QStringLiteral("%1 %2").arg(count).arg(p.label), widget);
    pill->setContentsMargins(6, 2, 6, 2);
    auto f = pill->font();
    f.setPointSize(f.pointSize() - 1);
    pill->setFont(f);
    pill->setStyleSheet(
        QStringLiteral("QLabel { background-color: rgba(%1,%2,%3,60); "
                       "color: rgb(%1,%2,%3); border-radius: 8px; "
                       "padding: 2px 6px; }")
            .arg(p.color.red()).arg(p.color.green()).arg(p.color.blue()));
    layout->addWidget(pill);
  }
  layout->addStretch();
  return widget;
}

// 8. get_session_cost -> mini cost dashboard
static QWidget *createSessionCostView(const QJsonObject &result,
                                      QWidget *parent) {
  double cost = result[QStringLiteral("total_cost_usd")].toDouble();

  auto *widget = new QWidget(parent);
  auto *grid = new QGridLayout(widget);
  grid->setContentsMargins(0, 0, 0, 0);
  grid->setSpacing(4);

  // Total cost (large, bold).
  auto *total_label = new QLabel(
      QStringLiteral("Total: $%1").arg(cost, 0, 'f', 2), widget);
  auto f = total_label->font();
  f.setPointSize(f.pointSize() + 2);
  f.setBold(true);
  total_label->setFont(f);
  grid->addWidget(total_label, 0, 0, 1, 2);

  int llm_calls = result[QStringLiteral("llm_calls")].toInt();
  int tool_calls = result[QStringLiteral("tool_calls")].toInt();
  int in_tok = result[QStringLiteral("total_input_tokens")].toInt();
  int out_tok = result[QStringLiteral("total_output_tokens")].toInt();

  auto small_font = widget->font();
  small_font.setPointSize(small_font.pointSize() - 1);

  auto *calls_label = new QLabel(
      QStringLiteral("LLM: %1 calls | Tools: %2 calls")
          .arg(llm_calls).arg(tool_calls), widget);
  calls_label->setFont(small_font);
  grid->addWidget(calls_label, 1, 0, 1, 2);

  auto *tokens_label = new QLabel(
      QStringLiteral("Tokens: %L1 in / %L2 out").arg(in_tok).arg(out_tok),
      widget);
  tokens_label->setFont(small_font);
  grid->addWidget(tokens_label, 2, 0, 1, 2);

  // Top tools by cost.
  auto by_tool = result[QStringLiteral("by_tool")].toArray();
  if (!by_tool.isEmpty()) {
    auto *tools_header = new QLabel(QStringLiteral("Top tools:"), widget);
    auto hf = tools_header->font();
    hf.setPointSize(hf.pointSize() - 1);
    hf.setBold(true);
    tools_header->setFont(hf);
    grid->addWidget(tools_header, 3, 0, 1, 2);

    int row = 4;
    auto limit = qMin(by_tool.size(), qsizetype(5));
    for (qsizetype i = 0; i < limit; ++i) {
      auto obj = by_tool[i].toObject();
      auto tool = obj[QStringLiteral("tool")].toString();
      int calls = obj[QStringLiteral("calls")].toInt();
      double tcost = obj[QStringLiteral("cost_usd")].toDouble();

      auto *name_lbl = new QLabel(tool, widget);
      name_lbl->setFont(small_font);
      grid->addWidget(name_lbl, row, 0);

      auto *cost_lbl = new QLabel(
          QStringLiteral("%1 calls, $%2")
              .arg(calls).arg(tcost, 0, 'f', 3), widget);
      cost_lbl->setFont(small_font);
      cost_lbl->setAlignment(Qt::AlignRight);
      grid->addWidget(cost_lbl, row, 1);
      ++row;
    }
  }
  return widget;
}

// 9. create_document -> clickable "Open document" button
static QWidget *createDocumentButton(const QJsonObject &result,
                                     QWidget *parent) {
  int doc_id = result[QStringLiteral("doc_id")].toInt(-1);
  if (doc_id < 0) {
    return nullptr;
  }
  auto title = result[QStringLiteral("title")].toString();
  auto *btn = new QPushButton(
      QStringLiteral("Open: %1").arg(
          title.isEmpty() ? QStringLiteral("Document %1").arg(doc_id) : title),
      parent);
  btn->setFlat(true);
  btn->setCursor(Qt::PointingHandCursor);
  btn->setStyleSheet(
      QStringLiteral("QPushButton { text-align: left; color: palette(link); }"));
  btn->setProperty("doc_id", doc_id);
  return btn;
}

// 10. create_findings_sheet / create_attack_surface_sheet / create_sheet
//     -> informational label (sheets auto-open in the Sheets panel)
static QWidget *createSheetLabel(const QJsonObject &result,
                                 const QJsonObject &args,
                                 QWidget *parent) {
  int sheet_id = result[QStringLiteral("sheet_id")].toInt(-1);
  if (sheet_id < 0) {
    return nullptr;
  }
  auto name = args[QStringLiteral("name")].toString();
  auto display = name.isEmpty()
                     ? QStringLiteral("Sheet %1").arg(sheet_id)
                     : name;
  auto *label = new QLabel(
      QStringLiteral("%1 \xe2\x80\x94 visible in the Sheets panel").arg(display),
      parent);
  auto f = label->font();
  f.setItalic(true);
  f.setPointSize(f.pointSize() - 1);
  label->setFont(f);
  label->setStyleSheet(QStringLiteral("color: palette(mid);"));
  return label;
}

// Scan text for C/C++ identifiers, resolve against the Index, and return
// text with markdown links for recognized entities.
static QString symbolizeIdentifiers(const QString &text,
                                    const mx::gui::ConfigManager &config) {
  if (text.isEmpty()) {
    return text;
  }

  // Match function-call-like identifiers: word(
  static const QRegularExpression func_re(
      QStringLiteral("\\b([a-zA-Z_][a-zA-Z0-9_]{2,})\\s*\\("));
  // Match snake_case identifiers (likely code symbols).
  static const QRegularExpression snake_re(
      QStringLiteral("\\b([a-z][a-z0-9]*(?:_[a-z0-9]+)+)\\b"));

  // Collect candidate identifiers with their positions.
  struct Candidate {
    int start;
    int length;
    QString name;
  };

  QHash<QString, uint64_t> cache;
  QVector<Candidate> candidates;

  auto try_resolve = [&](const QString &name) -> uint64_t {
    auto it = cache.find(name);
    if (it != cache.end()) {
      return it.value();
    }

    const auto &index = config.Index();
    uint64_t found_eid = 0;
    for (auto entity : index.query_entities(name.toStdString())) {
      if (std::holds_alternative<mx::NamedDecl>(entity)) {
        auto decl = std::get<mx::NamedDecl>(entity);
        found_eid = static_cast<uint64_t>(decl.id().Pack());
        break;
      }
    }
    cache.insert(name, found_eid);
    return found_eid;
  };

  // Gather candidates from both regexes.
  auto gather = [&](const QRegularExpression &re) {
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
      auto match = it.next();
      auto name = match.captured(1);
      candidates.append({static_cast<int>(match.capturedStart(1)),
                         static_cast<int>(match.capturedLength(1)),
                         name});
    }
  };

  gather(func_re);
  gather(snake_re);

  if (candidates.isEmpty()) {
    return text;
  }

  // Sort by position descending so we can replace from end to start.
  std::sort(candidates.begin(), candidates.end(),
            [](const Candidate &a, const Candidate &b) {
    return a.start > b.start;
  });

  // Deduplicate overlapping ranges (keep the first occurrence at each pos).
  QSet<int> seen_positions;
  QString result = text;
  for (const auto &c : candidates) {
    if (seen_positions.contains(c.start)) {
      continue;
    }
    seen_positions.insert(c.start);

    auto eid = try_resolve(c.name);
    if (eid == 0) {
      continue;
    }

    // Replace with markdown link: [name](entity:EID)
    auto link = QStringLiteral("[%1](entity:%2)").arg(c.name).arg(eid);
    result.replace(c.start, c.length, link);
  }
  return result;
}

// Regular expression matching annotated code fences in assistant messages.
// Formats:
//   ```fragment:12345\n...\n```
//   ```fragment:12345:10:25\n...\n```
//   ```entity:12345\n...\n```
static const QRegularExpression &annotatedCodeFenceRe(void) {
  static const QRegularExpression re(QStringLiteral(
      "```(?:fragment|entity):([0-9]+)(?::([0-9]+):([0-9]+))?\\n"
      "([\\s\\S]*?)```"));
  return re;
}

// Build syntax-highlighted HTML for a token range.  Clickable identifiers
// become <a href="entity:ID"> links; other tokens get <span> with the
// theme foreground color.  When `start_line`/`end_line` are non-zero only
// lines in that 1-based range are emitted.
static QString buildTokenHtml(const mx::TokenRange &tokens,
                              const mx::gui::ITheme &theme,
                              int start_line, int end_line) {
  QString html;
  int current_line = 1;
  bool filtering = (start_line > 0 && end_line > 0);
  auto in_range = [&]() {
    return !filtering || (current_line >= start_line &&
                          current_line <= end_line);
  };

  for (mx::Token tok : tokens) {
    auto cs = theme.TokenColorAndStyle(tok);
    if (!cs.foreground_color.isValid()) {
      cs.foreground_color = theme.DefaultForegroundColor();
    }

    auto tok_data = tok.data();
    QString text = QString::fromUtf8(
        tok_data.data(), static_cast<qsizetype>(tok_data.size()));

    // Handle newlines inside the token data — they advance current_line
    // and we need to emit only the parts that fall inside the line range.
    QStringList segments;
    qsizetype seg_start = 0;
    for (qsizetype i = 0; i < text.size(); ++i) {
      if (text[i] == QLatin1Char('\n')) {
        segments.append(text.mid(seg_start, i - seg_start + 1));
        seg_start = i + 1;
      }
    }
    if (seg_start < text.size()) {
      segments.append(text.mid(seg_start));
    }
    if (segments.isEmpty()) {
      segments.append(text);
    }

    auto related_eid = tok.related_entity_id().Pack();
    bool is_ident = (tok.kind() == mx::TokenKind::IDENTIFIER);

    for (const auto &seg : segments) {
      if (in_range()) {
        QString escaped = seg.toHtmlEscaped();
        if (related_eid != mx::kInvalidEntityId && is_ident) {
          html += QStringLiteral(
              "<a href='entity:%1' style='color:%2; "
              "text-decoration:none;'>%3</a>")
              .arg(QString::number(static_cast<quint64>(related_eid)),
                   cs.foreground_color.name(), escaped);
        } else {
          html += QStringLiteral("<span style='color:%1;'>%2</span>")
              .arg(cs.foreground_color.name(), escaped);
        }
      }
      if (seg.endsWith(QLatin1Char('\n'))) {
        ++current_line;
      }
    }
  }
  return html;
}

// Create a read-only QTextBrowser showing syntax-highlighted tokens for an
// annotated code block.  Returns nullptr if the entity cannot be resolved.
static QTextBrowser *createAnnotatedCodeView(
    const QString &type, uint64_t entity_id,
    int start_line, int end_line,
    const mx::gui::ConfigManager &config, QWidget *parent) {

  const auto &index = config.Index();
  auto entity = index.entity(mx::EntityId(entity_id));

  if (std::holds_alternative<mx::NotAnEntity>(entity)) {
    return nullptr;
  }

  // Get the token range for this entity.
  mx::TokenRange tokens;
  if (type == QStringLiteral("fragment")) {
    if (auto frag = std::get_if<mx::Fragment>(&entity)) {
      tokens = frag->parsed_tokens();
    }
  }
  // For any entity type, fall back to the generic Tokens() helper.
  if (tokens.empty()) {
    tokens = mx::gui::Tokens(entity);
  }
  if (tokens.empty()) {
    return nullptr;
  }

  auto theme = config.ThemeManager().Theme();
  if (!theme) {
    return nullptr;
  }

  auto token_html = buildTokenHtml(tokens, *theme, start_line, end_line);
  if (token_html.isEmpty()) {
    return nullptr;
  }

  auto bg = theme->DefaultBackgroundColor();
  auto font = theme->Font();

  QString full_html = QStringLiteral(
      "<pre style='margin:0; padding:6px; background:%1; "
      "font-family:%2; font-size:%3pt;'>%4</pre>")
      .arg(bg.name(), font.family(),
           QString::number(font.pointSize()), token_html);

  auto *browser = new QTextBrowser(parent);
  browser->setReadOnly(true);
  browser->setOpenLinks(false);
  browser->setHtml(full_html);
  browser->setFrameShape(QFrame::NoFrame);

  // Size to content, capped at 400px.
  auto doc_size = browser->document()->size().toSize();
  int h = qBound(30, doc_size.height() + 16, 400);
  browser->setFixedHeight(h);

  return browser;
}

// Check whether `content` contains any annotated code fences and, if so,
// split it into alternating markdown text and syntax-highlighted code view
// widgets inside `frame_layout`.  Returns true if annotated blocks were
// found and rendered.
static bool renderAnnotatedContent(
    const QString &content,
    QFrame *frame, QVBoxLayout *frame_layout,
    mx::gui::ConfigManager &config,
    std::function<void(const QString &)> on_link_activated) {

  const auto &re = annotatedCodeFenceRe();
  if (!re.match(content).hasMatch()) {
    return false;
  }

  auto it = re.globalMatch(content);

  // Collect matches with their positions.
  struct CodeBlock {
    int start;
    int length;
    QString type;         // "fragment" or "entity"
    uint64_t entity_id;
    int start_line;
    int end_line;
    QString fallback;     // raw text inside the fence
  };
  QVector<CodeBlock> blocks;

  while (it.hasNext()) {
    auto match = it.next();
    CodeBlock cb;
    cb.start = static_cast<int>(match.capturedStart());
    cb.length = static_cast<int>(match.capturedLength());

    // Detect whether the fence tag said "fragment" or "entity".
    auto full_tag = match.captured(0);
    if (full_tag.startsWith(QStringLiteral("```fragment:"))) {
      cb.type = QStringLiteral("fragment");
    } else {
      cb.type = QStringLiteral("entity");
    }

    cb.entity_id = match.captured(1).toULongLong();
    cb.start_line = match.captured(2).isEmpty() ? 0 : match.captured(2).toInt();
    cb.end_line = match.captured(3).isEmpty() ? 0 : match.captured(3).toInt();
    cb.fallback = match.captured(4);
    blocks.append(cb);
  }

  if (blocks.isEmpty()) {
    return false;
  }

  // Helper: create a QLabel for a markdown text chunk.
  auto make_md_label = [&](const QString &md) -> QLabel * {
    if (md.trimmed().isEmpty()) {
      return nullptr;
    }
    auto symbolized = symbolizeIdentifiers(md, config);
    auto *label = new QLabel(frame);
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    label->setTextFormat(Qt::MarkdownText);
    label->setText(symbolized);
    label->setTextInteractionFlags(Qt::TextBrowserInteraction);
    label->setOpenExternalLinks(false);
    QObject::connect(label, &QLabel::linkActivated, on_link_activated);
    return label;
  };

  int pos = 0;
  for (const auto &cb : blocks) {
    // Text chunk before this code block.
    if (cb.start > pos) {
      auto chunk = content.mid(pos, cb.start - pos);
      if (auto *label = make_md_label(chunk)) {
        frame_layout->addWidget(label);
      }
    }
    pos = cb.start + cb.length;

    // Try to create a syntax-highlighted view.
    auto *code_view = createAnnotatedCodeView(
        cb.type, cb.entity_id, cb.start_line, cb.end_line,
        config, frame);
    if (code_view) {
      // Store metadata for theme-change re-rendering.
      code_view->setProperty("mx_entity_id",
                             QVariant::fromValue(cb.entity_id));
      code_view->setProperty("mx_start_line", cb.start_line);
      code_view->setProperty("mx_end_line", cb.end_line);
      code_view->setProperty("mx_type", cb.type);

      QObject::connect(
          code_view, &QTextBrowser::anchorClicked, code_view,
          [on_link_activated](const QUrl &url) {
        on_link_activated(url.toString());
      });
      frame_layout->addWidget(code_view);
    } else {
      // Fallback: render the raw code as a plain code block.
      auto fallback_md = QStringLiteral("```\n") + cb.fallback +
                         QStringLiteral("```");
      if (auto *label = make_md_label(fallback_md)) {
        frame_layout->addWidget(label);
      }
    }
  }

  // Trailing text after the last code block.
  if (pos < content.size()) {
    auto tail = content.mid(pos);
    if (auto *label = make_md_label(tail)) {
      frame_layout->addWidget(label);
    }
  }

  return true;
}

// Dispatcher: returns an interactive widget for the tool result, or nullptr.
static QWidget *createToolResultWidget(const QString &tool_name,
                                       const QJsonObject &result,
                                       const QJsonObject &args,
                                       const mx::gui::ConfigManager &config,
                                       QWidget *parent) {
  // Don't create widgets for error results.
  if (result.contains(QStringLiteral("error"))) {
    return nullptr;
  }

  if (tool_name == QStringLiteral("list_files")) {
    return createListFilesView(result, config, parent);
  }
  if (tool_name == QStringLiteral("search_entities")) {
    return createSearchEntitiesView(result, config, parent);
  }
  if (tool_name == QStringLiteral("search_code")) {
    return createSearchCodeView(result, parent);
  }
  if (tool_name == QStringLiteral("get_references")) {
    return createReferencesView(result, parent);
  }
  if (tool_name == QStringLiteral("get_callers") ||
      tool_name == QStringLiteral("get_callees")) {
    return createCallTreeView(result, tool_name, parent);
  }
  if (tool_name == QStringLiteral("get_definition")) {
    return createDefinitionView(result, parent);
  }
  if (tool_name == QStringLiteral("get_task_board_summary")) {
    return createTaskBoardView(result, parent);
  }
  if (tool_name == QStringLiteral("get_session_cost")) {
    return createSessionCostView(result, parent);
  }
  if (tool_name == QStringLiteral("create_document")) {
    return createDocumentButton(result, parent);
  }
  if (tool_name == QStringLiteral("create_sheet") ||
      tool_name == QStringLiteral("create_findings_sheet") ||
      tool_name == QStringLiteral("create_attack_surface_sheet")) {
    return createSheetLabel(result, args, parent);
  }
  return nullptr;
}

}  // namespace

namespace mx::gui {

struct AgentConversationWidget::PrivateData {
  ThemeManager &theme_manager;
  ConfigManager &config_manager;

  QScrollArea *scroll_area{nullptr};
  QWidget *messages_container{nullptr};
  QVBoxLayout *messages_layout{nullptr};
  QPlainTextEdit *input_edit{nullptr};
  QPushButton *send_button{nullptr};
  QLabel *token_label{nullptr};

  // Theme colors.
  QColor user_bg;
  QColor assistant_bg;
  QColor system_fg;
  QColor tool_bg;
  QColor text_fg;

  QFrame *suggestion_frame{nullptr};
  QLabel *suggestion_text{nullptr};
  QLabel *suggestion_hint{nullptr};
  QPushButton *suggestion_more_btn{nullptr};
  QString current_suggestion;
  QStringList current_alternatives;

  // Status indicator.
  QLabel *status_label{nullptr};

  // Pending tool args keyed by tool_call_id for merging tool_call + tool_result.
  QMap<QString, QJsonObject> pending_tool_args;
  // Also store tool names for pending calls.
  QMap<QString, QString> pending_tool_names;

  // Tracked annotated code views for theme-change re-rendering.
  struct AnnotatedCodeRef {
    QTextBrowser *browser{nullptr};
    uint64_t entity_id{0};
    int start_line{0};
    int end_line{0};
  };
  QVector<AnnotatedCodeRef> annotated_code_views;

  bool auto_scroll{true};
  QPushButton *tail_btn{nullptr};

  int total_prompt_tokens{0};
  int total_completion_tokens{0};
  bool enter_to_send{true};

  explicit PrivateData(ThemeManager &tm, ConfigManager &cm)
      : theme_manager(tm), config_manager(cm) {}
};

AgentConversationWidget::~AgentConversationWidget(void) {}

void AgentConversationWidget::setEnterToSend(bool enabled) {
  d->enter_to_send = enabled;
}

AgentConversationWidget::AgentConversationWidget(ThemeManager &theme_manager,
                                                  ConfigManager &config_manager,
                                                  QWidget *parent)
    : QWidget(parent),
      d(new PrivateData(theme_manager, config_manager)) {

  auto *main_layout = new QVBoxLayout(this);
  main_layout->setContentsMargins(0, 0, 0, 0);
  main_layout->setSpacing(4);

  // Scrollable messages area.
  d->scroll_area = new QScrollArea(this);
  d->scroll_area->setWidgetResizable(true);
  d->scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  d->scroll_area->setFrameShape(QFrame::NoFrame);

  d->messages_container = new QWidget;
  d->messages_layout = new QVBoxLayout(d->messages_container);
  d->messages_layout->setContentsMargins(8, 8, 8, 8);
  d->messages_layout->setSpacing(8);
  d->messages_layout->addStretch();

  d->scroll_area->setWidget(d->messages_container);
  main_layout->addWidget(d->scroll_area, 1);

  // Floating "new messages" button overlaid on scroll area.
  d->tail_btn = new QPushButton(
      QStringLiteral("\xe2\x86\x93 New messages"), d->scroll_area);
  d->tail_btn->setStyleSheet(
      QStringLiteral("QPushButton { background: palette(highlight); "
                     "color: palette(highlighted-text); border-radius: 4px; "
                     "padding: 4px 8px; }"));
  d->tail_btn->setVisible(false);
  d->tail_btn->raise();

  connect(d->tail_btn, &QPushButton::clicked, this, [this] {
    d->auto_scroll = true;
    d->tail_btn->setVisible(false);
    auto *sb = d->scroll_area->verticalScrollBar();
    sb->setValue(sb->maximum());
  });

  connect(d->scroll_area->verticalScrollBar(), &QScrollBar::valueChanged,
          this, [this](int value) {
    auto *sb = d->scroll_area->verticalScrollBar();
    d->auto_scroll = (value >= sb->maximum() - 10);
    d->tail_btn->setVisible(!d->auto_scroll);
  });

  // Status indicator.
  d->status_label = new QLabel(this);
  d->status_label->setVisible(false);
  d->status_label->setContentsMargins(12, 2, 12, 2);
  {
    auto sf = d->status_label->font();
    sf.setItalic(true);
    sf.setPointSize(sf.pointSize() - 1);
    d->status_label->setFont(sf);
  }
  d->status_label->setStyleSheet(
      QStringLiteral("color: palette(placeholderText);"));
  main_layout->addWidget(d->status_label);

  // Suggestion box.
  d->suggestion_frame = new QFrame(this);
  d->suggestion_frame->setFrameShape(QFrame::StyledPanel);
  d->suggestion_frame->setStyleSheet(
      QStringLiteral("QFrame { background-color: palette(alternateBase); "
                     "border: 1px solid palette(mid); border-radius: 4px; }"));
  auto *sug_layout = new QHBoxLayout(d->suggestion_frame);
  sug_layout->setContentsMargins(8, 6, 8, 6);

  d->suggestion_text = new QLabel(d->suggestion_frame);
  d->suggestion_text->setWordWrap(true);
  sug_layout->addWidget(d->suggestion_text, 1);

  d->suggestion_hint = new QLabel(tr("Tab to accept"), d->suggestion_frame);
  auto hint_font = d->suggestion_hint->font();
  hint_font.setPointSize(hint_font.pointSize() - 1);
  d->suggestion_hint->setFont(hint_font);
  d->suggestion_hint->setStyleSheet(
      QStringLiteral("color: palette(mid);"));
  sug_layout->addWidget(d->suggestion_hint);

  d->suggestion_more_btn = new QPushButton(tr("more"), d->suggestion_frame);
  d->suggestion_more_btn->setFlat(true);
  sug_layout->addWidget(d->suggestion_more_btn);

  d->suggestion_frame->setVisible(false);
  main_layout->addWidget(d->suggestion_frame);

  // Input area.
  auto *input_layout = new QHBoxLayout;
  input_layout->setContentsMargins(4, 0, 4, 4);
  input_layout->setSpacing(4);

  d->input_edit = new QPlainTextEdit(this);
  d->input_edit->setPlaceholderText(tr("Type a message..."));
  d->input_edit->setMaximumHeight(80);
  d->input_edit->setTabChangesFocus(true);
  input_layout->addWidget(d->input_edit, 1);

  d->send_button = new QPushButton(tr("Send"), this);
  d->send_button->setFixedWidth(60);
  input_layout->addWidget(d->send_button, 0, Qt::AlignBottom);

  main_layout->addLayout(input_layout);

  // Token counter.
  d->token_label = new QLabel(tr("Tokens: 0 in / 0 out ($0.00)"), this);
  d->token_label->setContentsMargins(8, 0, 8, 4);
  auto font = d->token_label->font();
  font.setPointSize(font.pointSize() - 1);
  d->token_label->setFont(font);
  main_layout->addWidget(d->token_label);

  // Connections.
  connect(d->send_button, &QPushButton::clicked,
          this, &AgentConversationWidget::onSendClicked);
  connect(&theme_manager, &ThemeManager::ThemeChanged,
          this, &AgentConversationWidget::onThemeChanged);

  // Suggestion more button.
  connect(d->suggestion_more_btn, &QPushButton::clicked, this, [this] {
    if (d->current_alternatives.isEmpty()) {
      return;
    }
    auto *menu = new QMenu(this);
    for (const auto &alt : d->current_alternatives) {
      auto *action = menu->addAction(alt);
      connect(action, &QAction::triggered, this, [this, alt] {
        d->input_edit->setPlainText(alt);
        clearSuggestion();
      });
    }
    menu->popup(d->suggestion_more_btn->mapToGlobal(
        d->suggestion_more_btn->rect().bottomLeft()));
  });

  // Event filter on input for Tab-to-accept and scroll area for resize.
  d->input_edit->installEventFilter(this);
  d->scroll_area->installEventFilter(this);

  // Default suggestion.
  showSuggestion(
      tr("Search for attack surface entry points: functions that handle "
         "user input, network data, file parsing, or IPC. Create a "
         "findings sheet and an attack surface sheet to track results."),
      {tr("Search for TODO, FIXME, HACK, and XXX comments in the codebase "
          "to find developer-flagged areas of concern."),
       tr("Identify the main entry points and trace data flow inward. "
          "What are the first functions that touch untrusted input?"),
       tr("List all files and create a task board for a systematic "
          "audit of this codebase.")});

  applyThemeColors();
}

void AgentConversationWidget::addMessage(const AgentMessage &msg) {
  if (msg.role == QStringLiteral("tool_call")) {
    // Don't render tool_call; stash args for when tool_result arrives.
    if (!msg.tool_call_id.isEmpty()) {
      d->pending_tool_args[msg.tool_call_id] = msg.tool_args;
      d->pending_tool_names[msg.tool_call_id] = msg.tool_name;
    }
    return;
  }

  // For tool_result, look up the stashed args from the matching tool_call.
  QJsonObject merged_args = msg.tool_args;
  if (msg.role == QStringLiteral("tool_result") &&
      !msg.tool_call_id.isEmpty()) {
    auto it = d->pending_tool_args.find(msg.tool_call_id);
    if (it != d->pending_tool_args.end()) {
      merged_args = it.value();
      d->pending_tool_args.erase(it);
      d->pending_tool_names.remove(msg.tool_call_id);
    }
  }

  addMessageBubble(msg.role, msg.content, msg.tool_name,
                   merged_args, msg.tool_result);
}

void AgentConversationWidget::updateTokens(int prompt_tokens,
                                            int completion_tokens,
                                            double cost_usd) {
  d->total_prompt_tokens = prompt_tokens;
  d->total_completion_tokens = completion_tokens;

  double total_cost;
  if (cost_usd >= 0.0) {
    // Use authoritative cost from the rates table.
    total_cost = cost_usd;
  } else {
    // Estimate cost using typical API pricing (per 1M tokens).
    // Input: ~$3/M, Output: ~$15/M (approximate mid-range).
    double cost_in = d->total_prompt_tokens * 3.0 / 1000000.0;
    double cost_out = d->total_completion_tokens * 15.0 / 1000000.0;
    total_cost = cost_in + cost_out;
  }

  d->token_label->setText(
      tr("Tokens: %L1 in / %L2 out ($%3)")
          .arg(d->total_prompt_tokens)
          .arg(d->total_completion_tokens)
          .arg(total_cost, 0, 'f', 2));
}

void AgentConversationWidget::clear(void) {
  // Remove all message widgets (keep the stretch at index 0).
  while (d->messages_layout->count() > 1) {
    auto *item = d->messages_layout->takeAt(1);
    if (item->widget()) {
      delete item->widget();
    }
    delete item;
  }
  d->total_prompt_tokens = 0;
  d->total_completion_tokens = 0;
  d->token_label->setText(tr("Tokens: 0 in / 0 out ($0.00)"));
  d->pending_tool_args.clear();
  d->pending_tool_names.clear();
  d->auto_scroll = true;
  d->tail_btn->setVisible(false);
  clearStatus();
}

void AgentConversationWidget::setStatus(const QString &text) {
  d->status_label->setText(text);
  d->status_label->setVisible(true);
}

void AgentConversationWidget::clearStatus(void) {
  d->status_label->setVisible(false);
  d->status_label->clear();
}

void AgentConversationWidget::onSendClicked(void) {
  auto text = d->input_edit->toPlainText().trimmed();
  if (text.isEmpty()) {
    return;
  }
  d->input_edit->clear();
  clearSuggestion();
  emit sendMessageRequested(text);
}

void AgentConversationWidget::onThemeChanged(const ThemeManager &) {
  applyThemeColors();

  // Re-render annotated code views with the new theme colors.
  if (!d->messages_container) return;
  auto browsers = d->messages_container->findChildren<QTextBrowser *>();
  for (auto *browser : browsers) {
    auto eid_var = browser->property("mx_entity_id");
    if (!eid_var.isValid()) continue;

    auto eid = eid_var.toULongLong();
    auto start_line = browser->property("mx_start_line").toInt();
    auto end_line = browser->property("mx_end_line").toInt();
    auto type = browser->property("mx_type").toString();

    auto *fresh = createAnnotatedCodeView(
        type, eid, start_line, end_line, d->config_manager, browser->parentWidget());
    if (fresh) {
      // Replace the old browser's HTML with the fresh one.
      browser->setHtml(fresh->toHtml());
      fresh->deleteLater();
    }
  }
}

void AgentConversationWidget::addMessageBubble(
    const QString &role, const QString &content, const QString &tool_name,
    const QJsonObject &tool_args, const QJsonObject &tool_result) {

  auto *frame = new QFrame(d->messages_container);
  frame->setFrameShape(QFrame::StyledPanel);
  frame->setFrameShadow(QFrame::Plain);
  auto *frame_layout = new QVBoxLayout(frame);
  frame_layout->setContentsMargins(8, 6, 8, 6);
  frame_layout->setSpacing(4);

  auto make_label = [&](const QString &text, bool mono = false) -> QLabel * {
    auto *label = new QLabel(frame);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    label->setText(text);
    if (mono) {
      auto f = label->font();
      f.setFamily(QStringLiteral("monospace"));
      f.setPointSize(f.pointSize() - 1);
      label->setFont(f);
    }
    return label;
  };

  if (role == QStringLiteral("user")) {
    frame->setStyleSheet(
        QStringLiteral("QFrame { background-color: rgba(%1,%2,%3,%4); "
                       "border-radius: 8px; }")
            .arg(d->user_bg.red()).arg(d->user_bg.green())
            .arg(d->user_bg.blue()).arg(d->user_bg.alpha()));
    auto *label = make_label(content);
    label->setAlignment(Qt::AlignRight);
    frame_layout->addWidget(label);

    // Right-align user messages (slight indent from left).
    auto *wrapper = new QHBoxLayout;
    wrapper->addStretch(1);
    wrapper->addWidget(frame, 5);
    d->messages_layout->addLayout(wrapper);

  } else if (role == QStringLiteral("assistant")) {
    frame->setStyleSheet(
        QStringLiteral("QFrame { background-color: rgba(%1,%2,%3,%4); "
                       "border-radius: 8px; }")
            .arg(d->assistant_bg.red()).arg(d->assistant_bg.green())
            .arg(d->assistant_bg.blue()).arg(d->assistant_bg.alpha()));

    auto link_handler = [this](const QString &link) {
      if (link.startsWith(QStringLiteral("entity:"))) {
        emit navigateToEntity(link.mid(7).toULongLong());
      }
    };

    // Try the annotated code block path first.  If the content has
    // annotated fences (```fragment:ID or ```entity:ID) they are rendered
    // as syntax-highlighted, clickable code views interleaved with the
    // surrounding markdown.
    if (!renderAnnotatedContent(content, frame, frame_layout,
                                d->config_manager, link_handler)) {
      // Normal path: single QLabel with symbolized markdown.
      auto symbolized = symbolizeIdentifiers(content, d->config_manager);

      auto *label = new QLabel(frame);
      label->setWordWrap(true);
      label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
      label->setTextFormat(Qt::MarkdownText);
      label->setText(symbolized);
      label->setTextInteractionFlags(Qt::TextBrowserInteraction);
      label->setOpenExternalLinks(false);
      connect(label, &QLabel::linkActivated, this, link_handler);
      frame_layout->addWidget(label);
    }

    d->messages_layout->addWidget(frame);

  } else if (role == QStringLiteral("tool_result")) {
    frame->setStyleSheet(
        QStringLiteral("QFrame { background-color: rgba(%1,%2,%3,%4); "
                       "border-radius: 4px; border: 1px solid palette(mid); }")
            .arg(d->tool_bg.red()).arg(d->tool_bg.green())
            .arg(d->tool_bg.blue()).arg(d->tool_bg.alpha()));

    auto summary = toolResultSummary(tool_name, tool_result);
    auto *toggle_btn = new QPushButton(summary, frame);
    toggle_btn->setFlat(true);
    toggle_btn->setStyleSheet(
        QStringLiteral("QPushButton { text-align: left; }"));
    frame_layout->addWidget(toggle_btn);

    auto *detail = new QWidget(frame);
    detail->setVisible(false);
    auto *detail_layout = new QVBoxLayout(detail);
    detail_layout->setContentsMargins(4, 0, 4, 0);

    // Show args if available.
    if (!tool_args.isEmpty()) {
      auto args_text = formatToolArgs(tool_name, tool_args, d->config_manager);
      auto *args_label = make_label(
          QStringLiteral("Args: ") + args_text, true);
      detail_layout->addWidget(args_label);
    }

    auto *interactive = createToolResultWidget(
        tool_name, tool_result, tool_args, d->config_manager, detail);
    if (interactive) {
      detail_layout->addWidget(interactive);

      // Connect clickable views to navigate to entities on double-click.
      if (auto *list = qobject_cast<QListWidget *>(interactive)) {
        connect(list, &QListWidget::itemDoubleClicked, this,
                [this](QListWidgetItem *item) {
          auto eid = item->data(kEntityIdRole).toULongLong();
          if (eid != 0) {
            emit navigateToEntity(eid);
          }
        });
      } else if (auto *table = qobject_cast<QTableWidget *>(interactive)) {
        connect(table, &QTableWidget::itemDoubleClicked, this,
                [this](QTableWidgetItem *item) {
          auto eid = item->data(kEntityIdRole).toULongLong();
          if (eid != 0) {
            emit navigateToEntity(eid);
          }
        });
      } else if (auto *btn = qobject_cast<QPushButton *>(interactive)) {
        auto doc_id = btn->property("doc_id");
        if (doc_id.isValid()) {
          connect(btn, &QPushButton::clicked, this,
                  [this, did = doc_id.toInt()] {
            emit openDocument(did);
          });
        }
      }
    } else {
      auto formatted = tool_result.isEmpty()
                           ? content.left(2000)
                           : formatToolResult(tool_name, tool_result);
      auto *result_label = make_label(formatted.left(4000), true);
      detail_layout->addWidget(result_label);
    }

    frame_layout->addWidget(detail);

    connect(toggle_btn, &QPushButton::clicked, detail,
            [detail] { detail->setVisible(!detail->isVisible()); });

    d->messages_layout->addWidget(frame);

  } else {
    // System or unknown role -- centered, italic.
    frame->setStyleSheet(
        QStringLiteral("QFrame { border: none; }"));
    auto *label = make_label(content);
    label->setAlignment(Qt::AlignCenter);
    auto f = label->font();
    f.setItalic(true);
    label->setFont(f);
    label->setStyleSheet(
        QStringLiteral("color: %1;").arg(d->system_fg.name()));
    frame_layout->addWidget(label);
    d->messages_layout->addWidget(frame);
  }

  scrollToBottom();
}

void AgentConversationWidget::showSuggestion(
    const QString &suggestion, const QStringList &alternatives) {
  d->current_suggestion = suggestion;
  d->current_alternatives = alternatives;
  d->suggestion_text->setText(suggestion);
  d->suggestion_more_btn->setVisible(!alternatives.isEmpty());
  d->suggestion_frame->setVisible(true);
}

void AgentConversationWidget::clearSuggestion(void) {
  d->current_suggestion.clear();
  d->current_alternatives.clear();
  d->suggestion_frame->setVisible(false);
}

bool AgentConversationWidget::eventFilter(QObject *obj, QEvent *event) {
  if (obj == d->scroll_area && event->type() == QEvent::Resize) {
    // Reposition floating tail button at bottom-right of scroll area.
    auto w = d->tail_btn->sizeHint().width();
    auto h = d->tail_btn->sizeHint().height();
    auto sa = d->scroll_area->size();
    d->tail_btn->move(sa.width() - w - 16, sa.height() - h - 12);
  }
  if (obj == d->input_edit && event->type() == QEvent::KeyPress) {
    auto *key_event = static_cast<QKeyEvent *>(event);

    // Enter/Shift+Enter behavior depends on enter_to_send setting.
    if (key_event->key() == Qt::Key_Return ||
        key_event->key() == Qt::Key_Enter) {
      bool has_shift = key_event->modifiers() & Qt::ShiftModifier;
      bool should_send = d->enter_to_send ? !has_shift : has_shift;
      if (should_send) {
        onSendClicked();
        return true;
      }
    }

    // Tab accepts suggestion.
    if (key_event->key() == Qt::Key_Tab &&
        d->suggestion_frame->isVisible() &&
        !d->current_suggestion.isEmpty()) {
      d->input_edit->setPlainText(d->current_suggestion);
      auto cursor = d->input_edit->textCursor();
      cursor.movePosition(QTextCursor::End);
      d->input_edit->setTextCursor(cursor);
      auto suggestion = d->current_suggestion;
      clearSuggestion();
      emit suggestionAccepted(suggestion);
      return true;
    }

    // Typing dismisses suggestion.
    if (d->suggestion_frame->isVisible() &&
        d->input_edit->toPlainText().isEmpty() &&
        key_event->key() != Qt::Key_Tab &&
        key_event->key() != Qt::Key_Shift &&
        key_event->key() != Qt::Key_Control &&
        key_event->key() != Qt::Key_Alt &&
        key_event->key() != Qt::Key_Meta) {
      clearSuggestion();
    }
  }
  return QWidget::eventFilter(obj, event);
}

void AgentConversationWidget::scrollToBottom(void) {
  if (!d->auto_scroll) {
    // Show the tail button to indicate new content below.
    d->tail_btn->setVisible(true);
    return;
  }
  QTimer::singleShot(0, this, [this] {
    auto *bar = d->scroll_area->verticalScrollBar();
    bar->setValue(bar->maximum());
  });
}

void AgentConversationWidget::applyThemeColors(void) {
  auto theme = d->theme_manager.Theme();
  if (!theme) {
    d->user_bg = QColor(0x3B, 0x82, 0xF6, 0x40);
    d->assistant_bg = QColor(0x6B, 0x72, 0x80, 0x30);
    d->system_fg = QColor(0x9C, 0xA3, 0xAF);
    d->tool_bg = QColor(0x37, 0x41, 0x51, 0x40);
    d->text_fg = QColor(0xFF, 0xFF, 0xFF);
    return;
  }

  auto palette = theme->Palette();
  auto text = palette.color(QPalette::Text);
  auto base = palette.color(QPalette::Base);
  auto highlight = palette.color(QPalette::Highlight);

  d->text_fg = text;
  d->system_fg = palette.color(QPalette::PlaceholderText);

  // Detect light vs dark theme by background luminance.
  bool is_light = base.lightnessF() > 0.5;

  // User messages: accent-tinted.
  d->user_bg = highlight;
  d->user_bg.setAlpha(is_light ? 30 : 60);

  // Assistant messages: subtle contrast against the base.
  if (is_light) {
    // Light theme: slightly darker than background.
    d->assistant_bg = QColor(0, 0, 0, 15);
  } else {
    // Dark theme: slightly lighter than background.
    d->assistant_bg = QColor(255, 255, 255, 20);
  }

  // Tool calls/results: use alternateBase, ensure some contrast.
  d->tool_bg = palette.color(QPalette::AlternateBase);
  if (is_light) {
    // Ensure tool bg isn't pure white (invisible border).
    if (d->tool_bg.lightnessF() > 0.95) {
      d->tool_bg = QColor(0, 0, 0, 10);
    }
  }
}

}  // namespace mx::gui
