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

struct SpreadsheetToolContext {
  ConfigManager *config{nullptr};
};

// Register all spreadsheet tools with the given registry.
void registerSpreadsheetTools(AgentToolRegistry &registry,
                              SpreadsheetToolContext *ctx);

// ---------------------------------------------------------------------------
// Individual tool classes
// ---------------------------------------------------------------------------

class CreateSheetTool Q_DECL_FINAL : public AgentTool {
  SpreadsheetToolContext *m_ctx;
 public:
  explicit CreateSheetTool(SpreadsheetToolContext *ctx) : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

class ListSheetsTool Q_DECL_FINAL : public AgentTool {
  SpreadsheetToolContext *m_ctx;
 public:
  explicit ListSheetsTool(SpreadsheetToolContext *ctx) : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

class GetSheetSummaryTool Q_DECL_FINAL : public AgentTool {
  SpreadsheetToolContext *m_ctx;
 public:
  explicit GetSheetSummaryTool(SpreadsheetToolContext *ctx) : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

class ReadCellTool Q_DECL_FINAL : public AgentTool {
  SpreadsheetToolContext *m_ctx;
 public:
  explicit ReadCellTool(SpreadsheetToolContext *ctx) : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

class WriteCellTool Q_DECL_FINAL : public AgentTool {
  SpreadsheetToolContext *m_ctx;
 public:
  explicit WriteCellTool(SpreadsheetToolContext *ctx) : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

class ReadRowTool Q_DECL_FINAL : public AgentTool {
  SpreadsheetToolContext *m_ctx;
 public:
  explicit ReadRowTool(SpreadsheetToolContext *ctx) : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

class ReadColumnTool Q_DECL_FINAL : public AgentTool {
  SpreadsheetToolContext *m_ctx;
 public:
  explicit ReadColumnTool(SpreadsheetToolContext *ctx) : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

class AddRowTool Q_DECL_FINAL : public AgentTool {
  SpreadsheetToolContext *m_ctx;
 public:
  explicit AddRowTool(SpreadsheetToolContext *ctx) : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

class InsertRowTool Q_DECL_FINAL : public AgentTool {
  SpreadsheetToolContext *m_ctx;
 public:
  explicit InsertRowTool(SpreadsheetToolContext *ctx) : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

class DeleteRowTool Q_DECL_FINAL : public AgentTool {
  SpreadsheetToolContext *m_ctx;
 public:
  explicit DeleteRowTool(SpreadsheetToolContext *ctx) : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

class AddColumnTool Q_DECL_FINAL : public AgentTool {
  SpreadsheetToolContext *m_ctx;
 public:
  explicit AddColumnTool(SpreadsheetToolContext *ctx) : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

class SetRowColorTool Q_DECL_FINAL : public AgentTool {
  SpreadsheetToolContext *m_ctx;
 public:
  explicit SetRowColorTool(SpreadsheetToolContext *ctx) : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

class ClearRowColorTool Q_DECL_FINAL : public AgentTool {
  SpreadsheetToolContext *m_ctx;
 public:
  explicit ClearRowColorTool(SpreadsheetToolContext *ctx) : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

class SetCheckboxTool Q_DECL_FINAL : public AgentTool {
  SpreadsheetToolContext *m_ctx;
 public:
  explicit SetCheckboxTool(SpreadsheetToolContext *ctx) : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

class SortSheetTool Q_DECL_FINAL : public AgentTool {
  SpreadsheetToolContext *m_ctx;
 public:
  explicit SortSheetTool(SpreadsheetToolContext *ctx) : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

class ReadSheetRangeTool Q_DECL_FINAL : public AgentTool {
  SpreadsheetToolContext *m_ctx;
 public:
  explicit ReadSheetRangeTool(SpreadsheetToolContext *ctx) : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

class GetSheetAsMarkdownTool Q_DECL_FINAL : public AgentTool {
  SpreadsheetToolContext *m_ctx;
 public:
  explicit GetSheetAsMarkdownTool(SpreadsheetToolContext *ctx) : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

class WriteLocationCellTool Q_DECL_FINAL : public AgentTool {
  SpreadsheetToolContext *m_ctx;
 public:
  explicit WriteLocationCellTool(SpreadsheetToolContext *ctx) : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

class CreateFindingsSheetTool Q_DECL_FINAL : public AgentTool {
  SpreadsheetToolContext *m_ctx;
 public:
  explicit CreateFindingsSheetTool(SpreadsheetToolContext *ctx) : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

class CreateAttackSurfaceSheetTool Q_DECL_FINAL : public AgentTool {
  SpreadsheetToolContext *m_ctx;
 public:
  explicit CreateAttackSurfaceSheetTool(SpreadsheetToolContext *ctx)
      : m_ctx(ctx) {}
  QString name(void) const Q_DECL_FINAL;
  QString description(void) const Q_DECL_FINAL;
  QJsonObject parametersSchema(void) const Q_DECL_FINAL;
  QJsonObject execute(const QJsonObject &args) Q_DECL_FINAL;
};

}  // namespace mx::gui
