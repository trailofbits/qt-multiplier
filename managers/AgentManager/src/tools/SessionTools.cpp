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

// In-memory checkpoint/observation storage. Persistence comes in Step 10.
static int s_next_checkpoint_id = 1;
static int s_next_observation_id = 1;

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

  // Placeholder: in-memory ID assignment. Full persistence comes in Step 10.
  int checkpoint_id = s_next_checkpoint_id++;
  QString created_at = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

  QJsonObject result;
  result[QStringLiteral("checkpoint_id")] = checkpoint_id;
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

  // Placeholder: in-memory ID assignment. Full persistence comes in Step 10.
  int observation_id = s_next_observation_id++;
  QString timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

  QJsonObject result;
  result[QStringLiteral("observation_id")] = observation_id;
  result[QStringLiteral("timestamp")] = timestamp;
  return result;
}

// ===========================================================================
// Registration
// ===========================================================================

void registerSessionTools(AgentToolRegistry &registry,
                          SessionToolContext *ctx) {
  registry.registerTool(std::make_unique<GetAuditContextTool>(ctx));
  registry.registerTool(std::make_unique<SaveCheckpointTool>(ctx));
  registry.registerTool(std::make_unique<LogObservationTool>(ctx));
}

}  // namespace mx::gui
