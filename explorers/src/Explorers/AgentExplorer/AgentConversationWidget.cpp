// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "AgentConversationWidget.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QMap>
#include <QMenu>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

#include <multiplier/GUI/Interfaces/ITheme.h>
#include <multiplier/GUI/Managers/AgentMessage.h>
#include <multiplier/GUI/Managers/ThemeManager.h>

namespace {

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
    auto eid = static_cast<int64_t>(
        obj[QStringLiteral("entity_id")].toDouble());

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
      entry = QStringLiteral("%1 (%2:%3)%4")
                  .arg(name, kind)
                  .arg(eid)
                  .arg(loc);
    } else {
      entry = QStringLiteral("%1%2%3 (%4:%5)%6")
                  .arg(prefix, connector, name, kind)
                  .arg(eid)
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
    lines.append(QStringLiteral("entity_id  | kind     | name"));
    for (const auto &val : entities) {
      auto obj = val.toObject();
      auto eid = static_cast<int64_t>(
          obj[QStringLiteral("entity_id")].toDouble());
      auto kind = obj[QStringLiteral("kind")].toString();
      auto name = obj[QStringLiteral("name")].toString();
      lines.append(QStringLiteral("%1 | %2 | %3")
                       .arg(eid, -10)
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
                              const QJsonObject &args) {
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
    auto eid = static_cast<int64_t>(
        args[QStringLiteral("entity_id")].toDouble());
    if (eid != 0) {
      return QStringLiteral("entity_id: %1").arg(eid);
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

}  // namespace

namespace mx::gui {

struct AgentConversationWidget::PrivateData {
  ThemeManager &theme_manager;

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

  int total_prompt_tokens{0};
  int total_completion_tokens{0};
  bool enter_to_send{true};

  explicit PrivateData(ThemeManager &tm)
      : theme_manager(tm) {}
};

AgentConversationWidget::~AgentConversationWidget(void) {}

void AgentConversationWidget::setEnterToSend(bool enabled) {
  d->enter_to_send = enabled;
}

AgentConversationWidget::AgentConversationWidget(ThemeManager &theme_manager,
                                                  QWidget *parent)
    : QWidget(parent),
      d(new PrivateData(theme_manager)) {

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

  // Event filter on input for Tab-to-accept.
  d->input_edit->installEventFilter(this);

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

    // Right-align user messages.
    auto *wrapper = new QHBoxLayout;
    wrapper->addStretch(1);
    wrapper->addWidget(frame, 3);
    d->messages_layout->addLayout(wrapper);

  } else if (role == QStringLiteral("assistant")) {
    frame->setStyleSheet(
        QStringLiteral("QFrame { background-color: rgba(%1,%2,%3,%4); "
                       "border-radius: 8px; }")
            .arg(d->assistant_bg.red()).arg(d->assistant_bg.green())
            .arg(d->assistant_bg.blue()).arg(d->assistant_bg.alpha()));
    auto *label = make_label(content);
    frame_layout->addWidget(label);

    auto *wrapper = new QHBoxLayout;
    wrapper->addWidget(frame, 3);
    wrapper->addStretch(1);
    d->messages_layout->addLayout(wrapper);

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
      auto args_text = formatToolArgs(tool_name, tool_args);
      auto *args_label = make_label(
          QStringLiteral("Args: ") + args_text, true);
      detail_layout->addWidget(args_label);
    }

    auto formatted = tool_result.isEmpty()
                         ? content.left(2000)
                         : formatToolResult(tool_name, tool_result);
    auto *result_label = make_label(formatted.left(4000), true);
    detail_layout->addWidget(result_label);

    frame_layout->addWidget(detail);

    connect(toggle_btn, &QPushButton::clicked, detail,
            [detail] { detail->setVisible(!detail->isVisible()); });

    auto *wrapper = new QHBoxLayout;
    wrapper->addWidget(frame, 3);
    wrapper->addStretch(1);
    d->messages_layout->addLayout(wrapper);

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
