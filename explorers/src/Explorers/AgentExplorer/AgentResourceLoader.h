// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QFile>
#include <QRegularExpression>
#include <QString>
#include <QVector>

#include <multiplier/GUI/Managers/ConfigManager.h>

namespace mx::gui {

// Load a Qt resource file as a QString.
static inline QString loadResource(const QString &resource_path) {
  QFile f(resource_path);
  if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return QString::fromUtf8(f.readAll());
  }
  return {};
}

// Parse <!-- prompt-version: N --> from content.
static inline int parsePromptVersion(const QString &content) {
  static QRegularExpression re(
      QStringLiteral("<!--\\s*prompt-version:\\s*(\\d+)\\s*-->"));
  auto match = re.match(content);
  return match.hasMatch() ? match.captured(1).toInt() : 0;
}

// Ensure a document exists in the DB, creating or upgrading from resource
// content as needed.  Returns the document content (from DB if it exists
// and is up-to-date, otherwise from the resource).
static inline QString ensureResourceDocument(
    ConfigManager &config, const QString &title, const QString &category,
    const QString &resource_path, int version) {

  auto docs = config.LoadDocumentsByCategory(category);
  for (const auto &doc : docs) {
    if (doc.title == title) {
      auto existing = config.LoadDocumentContent(doc.doc_id);
      if (parsePromptVersion(existing) >= version) {
        return existing;
      }
      // Upgrade: replace with resource content.
      auto content = loadResource(resource_path);
      config.SaveDocumentContent(doc.doc_id, content);
      return content;
    }
  }

  // Create new document from resource.
  auto content = loadResource(resource_path);
  auto doc_id = config.CreateDocument(content, title,
                                      QStringLiteral("markdown"));
  if (doc_id >= 0) {
    config.SetDocumentCategory(doc_id, category);
  }
  return content;
}

// Struct describing a resource document to auto-create.
struct ResourceDocumentSpec {
  QString title;
  QString category;
  QString resource_path;
  int version;
};

// Ensure all resource documents exist.
static inline void ensureAllResourceDocuments(
    ConfigManager &config,
    const QVector<ResourceDocumentSpec> &specs) {
  for (const auto &spec : specs) {
    ensureResourceDocument(config, spec.title, spec.category,
                           spec.resource_path, spec.version);
  }
}

}  // namespace mx::gui
