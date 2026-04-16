// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "SessionTools.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QMetaObject>

namespace mx::gui {
namespace {

static QJsonObject error_result(const QString &msg) {
  QJsonObject r;
  r[QStringLiteral("error")] = msg;
  return r;
}

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

}  // namespace

// ===========================================================================
// GetAuditContextTool
// ===========================================================================

QString GetAuditContextTool::name(void) const {
  return QStringLiteral("get_audit_context");
}

QString GetAuditContextTool::description(void) const {
  return QStringLiteral(
      "Get a summary of the current session state: open sheets, documents, "
      "and general context for auditing.");
}

QJsonObject GetAuditContextTool::parametersSchema(void) const {
  return make_schema({}, {});
}

QJsonObject GetAuditContextTool::execute(const QJsonObject &) {
  // Sheets summary.
  QJsonArray sheets_arr;
  QVector<ConfigManager::SheetData> sheets;
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    sheets = m_ctx->config->LoadOpenSheets();
  }, Qt::BlockingQueuedConnection);
  for (const auto &sheet : sheets) {
    QJsonObject obj;
    obj[QStringLiteral("sheet_id")] = sheet.sheet_id;
    obj[QStringLiteral("name")] = sheet.name;
    obj[QStringLiteral("row_count")] = sheet.cells.size();
    obj[QStringLiteral("column_count")] = sheet.columns.size();
    sheets_arr.append(obj);
  }

  // Documents summary.
  QJsonArray docs_arr;
  QVector<ConfigManager::DocumentInfo> docs;
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    docs = m_ctx->config->LoadAllDocuments();
  }, Qt::BlockingQueuedConnection);
  for (const auto &doc : docs) {
    QJsonObject obj;
    obj[QStringLiteral("doc_id")] = doc.doc_id;
    obj[QStringLiteral("title")] = doc.title;
    docs_arr.append(obj);
  }

  QJsonObject result;
  result[QStringLiteral("sheets_summary")] = sheets_arr;
  result[QStringLiteral("documents_summary")] = docs_arr;
  result[QStringLiteral("open_sheets_count")] = sheets.size();
  result[QStringLiteral("documents_count")] = docs.size();
  return result;
}

// ===========================================================================
// SaveCheckpointTool
// ===========================================================================

QString SaveCheckpointTool::name(void) const {
  return QStringLiteral("save_checkpoint");
}

QString SaveCheckpointTool::description(void) const {
  return QStringLiteral(
      "Save a named checkpoint with a summary. "
      "Useful for tracking progress during a session.");
}

QJsonObject SaveCheckpointTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("summary")] = string_prop(
      QStringLiteral("Summary description of the checkpoint"));
  return make_schema(props, {QStringLiteral("summary")});
}

QJsonObject SaveCheckpointTool::execute(const QJsonObject &args) {
  QString summary = args[QStringLiteral("summary")].toString();
  if (summary.isEmpty()) {
    return error_result(QStringLiteral("summary is required"));
  }

  auto session_id = m_ctx->current_session_id;
  if (session_id < 0) {
    return error_result(QStringLiteral("No active session"));
  }

  int64_t checkpoint_id = -1;
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    checkpoint_id =
        m_ctx->config->SaveAgentCheckpoint(session_id, summary);
  }, Qt::BlockingQueuedConnection);
  QString created_at = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

  QJsonObject result;
  result[QStringLiteral("checkpoint_id")] =
      static_cast<qint64>(checkpoint_id);
  result[QStringLiteral("created_at")] = created_at;
  result[QStringLiteral("summary")] = summary;
  return result;
}

// ===========================================================================
// LogObservationTool
// ===========================================================================

QString LogObservationTool::name(void) const {
  return QStringLiteral("log_observation");
}

QString LogObservationTool::description(void) const {
  return QStringLiteral(
      "Log a timestamped observation during the session. "
      "Useful for forensic tracking.");
}

QJsonObject LogObservationTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("content")] = string_prop(
      QStringLiteral("Observation content to log"));
  return make_schema(props, {QStringLiteral("content")});
}

QJsonObject LogObservationTool::execute(const QJsonObject &args) {
  QString content = args[QStringLiteral("content")].toString();
  if (content.isEmpty()) {
    return error_result(QStringLiteral("content is required"));
  }

  auto session_id = m_ctx->current_session_id;
  if (session_id < 0) {
    return error_result(QStringLiteral("No active session"));
  }

  int64_t observation_id = -1;
  QMetaObject::invokeMethod(m_ctx->config, [&] {
    observation_id =
        m_ctx->config->SaveAgentObservation(session_id, content);
  }, Qt::BlockingQueuedConnection);
  QString timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

  QJsonObject result;
  result[QStringLiteral("observation_id")] =
      static_cast<qint64>(observation_id);
  result[QStringLiteral("timestamp")] = timestamp;
  return result;
}

// ===========================================================================
// GetSessionCostTool
// ===========================================================================

QString GetSessionCostTool::name(void) const {
  return QStringLiteral("get_session_cost");
}

QString GetSessionCostTool::description(void) const {
  return QStringLiteral(
      "Get a cost breakdown for the current session including total cost, "
      "per-tool costs, and per-role costs.");
}

QJsonObject GetSessionCostTool::parametersSchema(void) const {
  return make_schema({}, {});
}

QJsonObject GetSessionCostTool::execute(const QJsonObject &) {
  auto session_id = m_ctx->current_session_id;
  if (session_id < 0) {
    return error_result(QStringLiteral("No active session"));
  }

  ConfigManager::CostSummary summary;
  QVector<ConfigManager::ToolCostBreakdown> tool_breakdown;
  QVector<ConfigManager::RoleCostBreakdown> role_breakdown;

  QMetaObject::invokeMethod(m_ctx->config, [&] {
    summary = m_ctx->config->LoadCostSummary(session_id);
  }, Qt::BlockingQueuedConnection);

  QMetaObject::invokeMethod(m_ctx->config, [&] {
    tool_breakdown = m_ctx->config->LoadToolCostBreakdown(session_id);
  }, Qt::BlockingQueuedConnection);

  QMetaObject::invokeMethod(m_ctx->config, [&] {
    role_breakdown = m_ctx->config->LoadRoleCostBreakdown(session_id);
  }, Qt::BlockingQueuedConnection);

  QJsonObject result;
  result[QStringLiteral("total_cost_usd")] = summary.total_cost_usd;
  result[QStringLiteral("total_input_tokens")] = summary.total_input_tokens;
  result[QStringLiteral("total_output_tokens")] = summary.total_output_tokens;
  result[QStringLiteral("llm_calls")] = summary.llm_call_count;
  result[QStringLiteral("tool_calls")] = summary.tool_call_count;

  QJsonArray by_tool;
  for (const auto &t : tool_breakdown) {
    QJsonObject obj;
    obj[QStringLiteral("tool")] = t.tool_name;
    obj[QStringLiteral("calls")] = t.call_count;
    obj[QStringLiteral("cost_usd")] = t.total_cost_usd;
    obj[QStringLiteral("avg_duration_ms")] = t.avg_duration_ms;
    by_tool.append(obj);
  }
  result[QStringLiteral("by_tool")] = by_tool;

  QJsonArray by_role;
  for (const auto &r : role_breakdown) {
    QJsonObject obj;
    obj[QStringLiteral("role")] = r.node_type;
    obj[QStringLiteral("model")] = r.model;
    obj[QStringLiteral("calls")] = r.call_count;
    obj[QStringLiteral("cost_usd")] = r.total_cost_usd;
    by_role.append(obj);
  }
  result[QStringLiteral("by_role")] = by_role;

  return result;
}

// ===========================================================================
// FinishTool
// ===========================================================================


QString FinishTool::name(void) const {
  return QStringLiteral("finish");
}

QString FinishTool::description(void) const {
  return QStringLiteral(
      "Call when you are done with your current work. Provide a summary of "
      "what was accomplished, optional next actions, and a status.");
}

QJsonObject FinishTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("summary")] = string_prop(
      QStringLiteral("Summary of what was accomplished"));

  QJsonObject next_actions_prop;
  next_actions_prop[QStringLiteral("type")] = QStringLiteral("array");
  next_actions_prop[QStringLiteral("description")] =
      QStringLiteral("Suggested follow-up actions");
  QJsonObject items;
  items[QStringLiteral("type")] = QStringLiteral("string");
  next_actions_prop[QStringLiteral("items")] = items;
  props[QStringLiteral("next_actions")] = next_actions_prop;

  QJsonObject status_prop;
  status_prop[QStringLiteral("type")] = QStringLiteral("string");
  status_prop[QStringLiteral("description")] =
      QStringLiteral("Status: completed, blocked, or needs_input");
  status_prop[QStringLiteral("enum")] = QJsonArray{
      QStringLiteral("completed"), QStringLiteral("blocked"),
      QStringLiteral("needs_input")};
  props[QStringLiteral("status")] = status_prop;

  return make_schema(props, {QStringLiteral("summary")});
}

QJsonObject FinishTool::execute(const QJsonObject &args) {
  QString summary = args[QStringLiteral("summary")].toString();
  if (summary.isEmpty()) {
    return error_result(QStringLiteral("summary is required"));
  }

  SessionResult result;
  result.summary = summary;
  result.status = args[QStringLiteral("status")].toString(
      QStringLiteral("completed"));

  auto actions = args[QStringLiteral("next_actions")].toArray();
  for (const auto &a : actions) {
    auto s = a.toString();
    if (!s.isEmpty()) {
      result.next_actions.append(s);
    }
  }

  m_ctx->finish_result = result;
  m_ctx->finish_called = true;

  QJsonObject r;
  r[QStringLiteral("acknowledged")] = true;
  return r;
}

// ===========================================================================
// Registration
// ===========================================================================

void registerSessionTools(AgentToolRegistry &registry,
                          SessionToolContext *ctx) {
  registry.registerTool(std::make_unique<GetAuditContextTool>(ctx));
  registry.registerTool(std::make_unique<SaveCheckpointTool>(ctx));
  registry.registerTool(std::make_unique<LogObservationTool>(ctx));
  registry.registerTool(std::make_unique<GetSessionCostTool>(ctx));
  registry.registerTool(std::make_unique<FinishTool>(ctx));
}

}  // namespace mx::gui
