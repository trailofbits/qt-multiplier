// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "DocumentTools.h"

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
// CreateDocumentTool
// ===========================================================================

QString CreateDocumentTool::name(void) const {
  return QStringLiteral("create_document");
}

QString CreateDocumentTool::description(void) const {
  return QStringLiteral(
      "Create a new document. Categories: note (default), prompt, "
      "report_template, skill_template, custom.");
}

QJsonObject CreateDocumentTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("title")] = string_prop(
      QStringLiteral("Title of the document"));
  props[QStringLiteral("content")] = string_prop(
      QStringLiteral("Initial content (optional)"));
  props[QStringLiteral("category")] = string_prop(
      QStringLiteral("Category: note, prompt, report_template, "
                      "skill_template, custom (default: note)"));
  return make_schema(props, {QStringLiteral("title")});
}

QJsonObject CreateDocumentTool::execute(const QJsonObject &args) {
  QString title = args[QStringLiteral("title")].toString();
  if (title.isEmpty()) {
    return error_result(QStringLiteral("title is required"));
  }

  QString content = args[QStringLiteral("content")].toString();
  QString category = args[QStringLiteral("category")].toString(
      QStringLiteral("note"));

  // Validate category.
  static const QStringList kValidCategories = {
      QStringLiteral("note"), QStringLiteral("prompt"),
      QStringLiteral("report_template"), QStringLiteral("skill_template"),
      QStringLiteral("custom")};
  if (!kValidCategories.contains(category)) {
    return error_result(
        QStringLiteral("invalid category: %1").arg(category));
  }

  int doc_id = m_ctx->config->CreateDocument(content, title);
  if (doc_id < 0) {
    return error_result(QStringLiteral("failed to create document"));
  }

  // Store category in the description field for now.
  m_ctx->config->SaveDocumentDescription(doc_id, category);
  QMetaObject::invokeMethod(m_ctx->config,
      &ConfigManager::NotifyExternalDocumentsChanged, Qt::QueuedConnection);

  QJsonObject result;
  result[QStringLiteral("doc_id")] = doc_id;
  result[QStringLiteral("title")] = title;
  return result;
}

// ===========================================================================
// ReadDocumentTool
// ===========================================================================

QString ReadDocumentTool::name(void) const {
  return QStringLiteral("read_document");
}

QString ReadDocumentTool::description(void) const {
  return QStringLiteral("Read a document by its ID.");
}

QJsonObject ReadDocumentTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("doc_id")] = int_prop(
      QStringLiteral("Document ID"));
  return make_schema(props, {QStringLiteral("doc_id")});
}

QJsonObject ReadDocumentTool::execute(const QJsonObject &args) {
  int doc_id = args[QStringLiteral("doc_id")].toInt(-1);
  if (doc_id < 0) {
    return error_result(QStringLiteral("doc_id is required"));
  }

  QString title = m_ctx->config->LoadDocumentTitle(doc_id);
  QString content = m_ctx->config->LoadDocumentContent(doc_id);

  QJsonObject result;
  result[QStringLiteral("doc_id")] = doc_id;
  result[QStringLiteral("title")] = title;
  result[QStringLiteral("content")] = content;
  return result;
}

// ===========================================================================
// EditDocumentTool
// ===========================================================================

QString EditDocumentTool::name(void) const {
  return QStringLiteral("edit_document");
}

QString EditDocumentTool::description(void) const {
  return QStringLiteral(
      "Edit a document's content and/or title. "
      "Mode: replace (default) or append.");
}

QJsonObject EditDocumentTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("doc_id")] = int_prop(
      QStringLiteral("Document ID"));
  props[QStringLiteral("content")] = string_prop(
      QStringLiteral("New content (optional)"));
  props[QStringLiteral("title")] = string_prop(
      QStringLiteral("New title (optional)"));
  props[QStringLiteral("mode")] = string_prop(
      QStringLiteral("Edit mode: replace (default) or append"));
  return make_schema(props, {QStringLiteral("doc_id")});
}

QJsonObject EditDocumentTool::execute(const QJsonObject &args) {
  int doc_id = args[QStringLiteral("doc_id")].toInt(-1);
  if (doc_id < 0) {
    return error_result(QStringLiteral("doc_id is required"));
  }

  QString mode = args[QStringLiteral("mode")].toString(
      QStringLiteral("replace"));

  // Edit content if provided. Write directly to DB, bypassing undo stack.
  if (args.contains(QStringLiteral("content"))) {
    QString new_content = args[QStringLiteral("content")].toString();
    if (mode == QLatin1String("append")) {
      QString existing = m_ctx->config->LoadDocumentContent(doc_id);
      new_content = existing + new_content;
    }
    m_ctx->config->SaveDocumentContent(doc_id, new_content);
  }

  // Edit title if provided. Write directly to DB, bypassing undo stack.
  if (args.contains(QStringLiteral("title"))) {
    QString new_title = args[QStringLiteral("title")].toString();
    m_ctx->config->SaveDocumentTitle(doc_id, new_title);
    ConfigManager::BumpDocumentTitleVersion();
  }

  QMetaObject::invokeMethod(m_ctx->config,
      &ConfigManager::NotifyExternalDocumentsChanged, Qt::QueuedConnection);

  QJsonObject result;
  result[QStringLiteral("success")] = true;
  return result;
}

// ===========================================================================
// ListDocumentsTool
// ===========================================================================

QString ListDocumentsTool::name(void) const {
  return QStringLiteral("list_documents");
}

QString ListDocumentsTool::description(void) const {
  return QStringLiteral(
      "List all documents. Optionally filter by category.");
}

QJsonObject ListDocumentsTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("category")] = string_prop(
      QStringLiteral("Optional category filter"));
  return make_schema(props, {});
}

QJsonObject ListDocumentsTool::execute(const QJsonObject &args) {
  QString category_filter = args[QStringLiteral("category")].toString();

  auto docs = m_ctx->config->LoadAllDocuments();

  QJsonArray arr;
  for (const auto &doc : docs) {
    // The description field stores the category.
    if (!category_filter.isEmpty() && doc.description != category_filter) {
      continue;
    }

    QJsonObject obj;
    obj[QStringLiteral("doc_id")] = doc.doc_id;
    obj[QStringLiteral("title")] = doc.title;
    obj[QStringLiteral("description")] = doc.description;
    obj[QStringLiteral("category")] = doc.description;
    obj[QStringLiteral("created_at")] = doc.created_at;
    arr.append(obj);
  }

  QJsonObject result;
  result[QStringLiteral("documents")] = arr;
  return result;
}

// ===========================================================================
// LinkDocumentToCellTool
// ===========================================================================

QString LinkDocumentToCellTool::name(void) const {
  return QStringLiteral("link_document_to_cell");
}

QString LinkDocumentToCellTool::description(void) const {
  return QStringLiteral(
      "Link a document to a sheet cell, creating a DocumentCell reference.");
}

QJsonObject LinkDocumentToCellTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("sheet_id")] = int_prop(
      QStringLiteral("Sheet ID"));
  props[QStringLiteral("row")] = int_prop(
      QStringLiteral("Row index (0-based)"));
  props[QStringLiteral("column")] = int_prop(
      QStringLiteral("Column index (0-based)"));
  props[QStringLiteral("doc_id")] = int_prop(
      QStringLiteral("Document ID to link"));
  return make_schema(props, {QStringLiteral("sheet_id"),
                             QStringLiteral("row"),
                             QStringLiteral("column"),
                             QStringLiteral("doc_id")});
}

QJsonObject LinkDocumentToCellTool::execute(const QJsonObject &args) {
  int sheet_id = args[QStringLiteral("sheet_id")].toInt(-1);
  int row = args[QStringLiteral("row")].toInt(-1);
  int col = args[QStringLiteral("column")].toInt(-1);
  int doc_id = args[QStringLiteral("doc_id")].toInt(-1);

  if (sheet_id < 0) {
    return error_result(QStringLiteral("sheet_id is required"));
  }
  if (row < 0) {
    return error_result(QStringLiteral("row is required"));
  }
  if (col < 0) {
    return error_result(QStringLiteral("column is required"));
  }
  if (doc_id < 0) {
    return error_result(QStringLiteral("doc_id is required"));
  }

  auto sheet = m_ctx->config->LoadSheetById(sheet_id);
  if (sheet.sheet_id < 0) {
    return error_result(QStringLiteral("sheet not found"));
  }
  if (col >= sheet.columns.size()) {
    return error_result(QStringLiteral("column out of range"));
  }

  // Ensure row exists.
  while (sheet.cells.size() <= row) {
    sheet.cells.append(QVector<QString>(sheet.columns.size()));
  }
  auto &r = sheet.cells[row];
  while (r.size() <= col) {
    r.append(QString());
  }

  // Load document title for the cell display cache.
  QString title = m_ctx->config->LoadDocumentTitle(doc_id);
  QString escaped_title = title;
  escaped_title.replace(QLatin1Char('"'), QStringLiteral("\\\""));

  // Write the DocumentCell JSON format: {"t":"doc","id":<id>,"tl":"<title>"}
  r[col] = QStringLiteral("{\"t\":\"doc\",\"id\":%1,\"tl\":\"%2\"}")
               .arg(doc_id)
               .arg(escaped_title);

  m_ctx->config->SaveSheet(sheet);
  QMetaObject::invokeMethod(m_ctx->config,
      &ConfigManager::NotifyExternalSheetsChanged, Qt::QueuedConnection);

  QJsonObject result;
  result[QStringLiteral("success")] = true;
  return result;
}

// ===========================================================================
// SearchDocumentsTool
// ===========================================================================

QString SearchDocumentsTool::name(void) const {
  return QStringLiteral("search_documents");
}

QString SearchDocumentsTool::description(void) const {
  return QStringLiteral(
      "Search documents by title substring (case-insensitive).");
}

QJsonObject SearchDocumentsTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("query")] = string_prop(
      QStringLiteral("Search query string"));
  return make_schema(props, {QStringLiteral("query")});
}

QJsonObject SearchDocumentsTool::execute(const QJsonObject &args) {
  QString query = args[QStringLiteral("query")].toString();
  if (query.isEmpty()) {
    return error_result(QStringLiteral("query is required"));
  }

  auto docs = m_ctx->config->LoadAllDocuments();

  QJsonArray arr;
  for (const auto &doc : docs) {
    if (doc.title.contains(query, Qt::CaseInsensitive)) {
      QJsonObject obj;
      obj[QStringLiteral("doc_id")] = doc.doc_id;
      obj[QStringLiteral("title")] = doc.title;
      arr.append(obj);
    }
  }

  QJsonObject result;
  result[QStringLiteral("matches")] = arr;
  return result;
}

// ===========================================================================
// Registration
// ===========================================================================

void registerDocumentTools(AgentToolRegistry &registry,
                           DocumentToolContext *ctx) {
  registry.registerTool(std::make_unique<CreateDocumentTool>(ctx));
  registry.registerTool(std::make_unique<ReadDocumentTool>(ctx));
  registry.registerTool(std::make_unique<EditDocumentTool>(ctx));
  registry.registerTool(std::make_unique<ListDocumentsTool>(ctx));
  registry.registerTool(std::make_unique<LinkDocumentToCellTool>(ctx));
  registry.registerTool(std::make_unique<SearchDocumentsTool>(ctx));
}

}  // namespace mx::gui
