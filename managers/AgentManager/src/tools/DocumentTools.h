// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Managers/ConfigManager.h>

#include "AgentTool.h"
#include "AgentToolRegistry.h"

namespace mx::gui {

struct DocumentToolContext {
  ConfigManager *config{nullptr};
};

// Register all document tools with the given registry.
void registerDocumentTools(AgentToolRegistry &registry,
                           DocumentToolContext *ctx);

// ---------------------------------------------------------------------------
// Individual tool classes
// ---------------------------------------------------------------------------

class CreateDocumentTool Q_DECL_FINAL : public AgentTool {
  DocumentToolContext *m_ctx;
 public:
  explicit CreateDocumentTool(DocumentToolContext *ctx) : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

class ReadDocumentTool Q_DECL_FINAL : public AgentTool {
  DocumentToolContext *m_ctx;
 public:
  explicit ReadDocumentTool(DocumentToolContext *ctx) : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

class EditDocumentTool Q_DECL_FINAL : public AgentTool {
  DocumentToolContext *m_ctx;
 public:
  explicit EditDocumentTool(DocumentToolContext *ctx) : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

class ListDocumentsTool Q_DECL_FINAL : public AgentTool {
  DocumentToolContext *m_ctx;
 public:
  explicit ListDocumentsTool(DocumentToolContext *ctx) : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

class LinkDocumentToCellTool Q_DECL_FINAL : public AgentTool {
  DocumentToolContext *m_ctx;
 public:
  explicit LinkDocumentToCellTool(DocumentToolContext *ctx) : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

class SearchDocumentsTool Q_DECL_FINAL : public AgentTool {
  DocumentToolContext *m_ctx;
 public:
  explicit SearchDocumentsTool(DocumentToolContext *ctx) : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

}  // namespace mx::gui
