// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "SessionTools.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>

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
  auto sheets = m_ctx->config->LoadOpenSheets();
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
  auto docs = m_ctx->config->LoadAllDocuments();
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

  auto checkpoint_id =
      m_ctx->config->SaveAgentCheckpoint(session_id, summary);
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

  auto observation_id =
      m_ctx->config->SaveAgentObservation(session_id, content);
  QString timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

  QJsonObject result;
  result[QStringLiteral("observation_id")] =
      static_cast<qint64>(observation_id);
  result[QStringLiteral("timestamp")] = timestamp;
  return result;
}

// ===========================================================================
// FinishTool
// ===========================================================================

SessionResult FinishTool::s_last_result;
bool FinishTool::s_was_called = false;

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

  s_last_result = result;
  s_was_called = true;

  QJsonObject r;
  r[QStringLiteral("acknowledged")] = true;
  return r;
}

SessionResult FinishTool::lastResult(void) {
  return s_last_result;
}

bool FinishTool::wasCalledAndReset(void) {
  if (s_was_called) {
    s_was_called = false;
    return true;
  }
  return false;
}

// ===========================================================================
// Registration
// ===========================================================================

void registerSessionTools(AgentToolRegistry &registry,
                          SessionToolContext *ctx) {
  registry.registerTool(std::make_unique<GetAuditContextTool>(ctx));
  registry.registerTool(std::make_unique<SaveCheckpointTool>(ctx));
  registry.registerTool(std::make_unique<LogObservationTool>(ctx));
  registry.registerTool(std::make_unique<FinishTool>(ctx));
}

}  // namespace mx::gui
