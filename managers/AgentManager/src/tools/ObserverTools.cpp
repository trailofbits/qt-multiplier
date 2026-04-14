// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "ObserverTools.h"

#include <multiplier/GUI/Managers/AgentManager.h>

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace mx::gui {
namespace {

static int s_next_recommendation_id = 1;

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

static QJsonObject int_prop(const QString &desc) {
  QJsonObject p;
  p[QStringLiteral("type")] = QStringLiteral("integer");
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
// GetPrimarySessionContextTool
// ===========================================================================

QString GetPrimarySessionContextTool::name(void) const {
  return QStringLiteral("get_primary_session_context");
}

QString GetPrimarySessionContextTool::description(void) const {
  return QStringLiteral(
      "Read the primary agent's recent messages and tool calls to understand "
      "its progress and approach.");
}

QJsonObject GetPrimarySessionContextTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("last_n_messages")] = int_prop(
      QStringLiteral("Number of recent messages to retrieve (default 20)"));
  return make_schema(props, {});
}

QJsonObject GetPrimarySessionContextTool::execute(const QJsonObject &args) {
  if (!m_ctx || !m_ctx->agent_manager) {
    return error_result(QStringLiteral("Observer context not configured"));
  }

  auto primary_id = m_ctx->primary_session_id;
  if (primary_id < 0) {
    return error_result(QStringLiteral("No primary session configured"));
  }

  qsizetype last_n = args[QStringLiteral("last_n_messages")].toInt(20);
  if (last_n <= 0) {
    last_n = 20;
  }

  auto all_messages = m_ctx->agent_manager->sessionMessages(primary_id);

  // Take the last N messages.
  auto start = qMax(qsizetype{0}, all_messages.size() - last_n);
  QJsonArray messages_arr;
  for (qsizetype i = start; i < all_messages.size(); ++i) {
    const auto &msg = all_messages[i];
    QJsonObject obj;
    obj[QStringLiteral("role")] = msg.role;
    obj[QStringLiteral("content")] = msg.content;
    if (!msg.tool_name.isEmpty()) {
      obj[QStringLiteral("tool_name")] = msg.tool_name;
    }
    if (!msg.tool_result.isEmpty()) {
      obj[QStringLiteral("tool_result")] =
          QString::fromUtf8(QJsonDocument(msg.tool_result)
                                .toJson(QJsonDocument::Compact))
              .left(500);
    }
    messages_arr.append(obj);
  }

  // Summarize sheets and documents.
  QJsonArray sheets_arr;
  QJsonArray docs_arr;
  if (m_ctx->config) {
    auto sheets = m_ctx->config->LoadOpenSheets();
    for (const auto &sheet : sheets) {
      QJsonObject obj;
      obj[QStringLiteral("sheet_id")] = sheet.sheet_id;
      obj[QStringLiteral("name")] = sheet.name;
      obj[QStringLiteral("row_count")] = sheet.cells.size();
      sheets_arr.append(obj);
    }

    auto docs = m_ctx->config->LoadAllDocuments();
    for (const auto &doc : docs) {
      QJsonObject obj;
      obj[QStringLiteral("doc_id")] = doc.doc_id;
      obj[QStringLiteral("title")] = doc.title;
      docs_arr.append(obj);
    }
  }

  QJsonObject result;
  result[QStringLiteral("messages")] = messages_arr;
  result[QStringLiteral("total_messages")] = all_messages.size();
  result[QStringLiteral("sheets_summary")] = sheets_arr;
  result[QStringLiteral("documents_summary")] = docs_arr;
  return result;
}

// ===========================================================================
// ObserverRecommendationTool
// ===========================================================================

QString ObserverRecommendationTool::name(void) const {
  return QStringLiteral("observer_recommendation");
}

QString ObserverRecommendationTool::description(void) const {
  return QStringLiteral(
      "Write a recommendation to the observer notes document. "
      "Each recommendation is timestamped and prioritized.");
}

QJsonObject ObserverRecommendationTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("recommendation")] = string_prop(
      QStringLiteral("The recommendation text"));

  QJsonObject priority_prop;
  priority_prop[QStringLiteral("type")] = QStringLiteral("string");
  priority_prop[QStringLiteral("description")] =
      QStringLiteral("Priority level: high, medium, or low");
  QJsonArray enum_vals;
  enum_vals.append(QStringLiteral("high"));
  enum_vals.append(QStringLiteral("medium"));
  enum_vals.append(QStringLiteral("low"));
  priority_prop[QStringLiteral("enum")] = enum_vals;
  props[QStringLiteral("priority")] = priority_prop;

  return make_schema(props, {QStringLiteral("recommendation")});
}

QJsonObject ObserverRecommendationTool::execute(const QJsonObject &args) {
  QString recommendation =
      args[QStringLiteral("recommendation")].toString();
  if (recommendation.isEmpty()) {
    return error_result(QStringLiteral("recommendation is required"));
  }

  QString priority =
      args[QStringLiteral("priority")].toString(QStringLiteral("medium"));
  if (priority != QStringLiteral("high") &&
      priority != QStringLiteral("medium") &&
      priority != QStringLiteral("low")) {
    priority = QStringLiteral("medium");
  }

  QString timestamp =
      QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

  // Format the recommendation with timestamp and priority.
  QString formatted = QStringLiteral("[%1] [%2] %3")
                          .arg(timestamp, priority.toUpper(), recommendation);

  int doc_id = -1;
  if (m_ctx && m_ctx->config) {
    // Find or create the observer_notes document.
    auto docs = m_ctx->config->LoadDocumentsByCategory(
        QStringLiteral("observer_notes"));
    if (!docs.isEmpty()) {
      doc_id = docs.first().doc_id;
      // Append to existing content.
      auto existing = m_ctx->config->LoadDocumentContent(doc_id);
      if (!existing.isEmpty()) {
        existing += QStringLiteral("\n");
      }
      existing += formatted;
      m_ctx->config->SaveDocumentContent(doc_id, existing);
    } else {
      // Create a new observer notes document.
      doc_id = m_ctx->config->CreateDocument(
          formatted, QStringLiteral("Observer Notes"));
      m_ctx->config->SetDocumentCategory(doc_id,
                                         QStringLiteral("observer_notes"));
    }
  }

  int recommendation_id = s_next_recommendation_id++;

  QJsonObject result;
  result[QStringLiteral("doc_id")] = doc_id;
  result[QStringLiteral("recommendation_id")] = recommendation_id;
  result[QStringLiteral("timestamp")] = timestamp;
  result[QStringLiteral("priority")] = priority;
  return result;
}

// ===========================================================================
// Registration
// ===========================================================================

void registerObserverTools(AgentToolRegistry &registry,
                           ObserverToolContext *ctx) {
  registry.registerTool(
      std::make_unique<GetPrimarySessionContextTool>(ctx));
  registry.registerTool(
      std::make_unique<ObserverRecommendationTool>(ctx));
}

}  // namespace mx::gui
