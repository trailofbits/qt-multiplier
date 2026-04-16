// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "NavigationTools.h"

#include <QJsonArray>
#include <QJsonObject>

#include <multiplier/Index.h>
#include <multiplier/Reference.h>

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

// Get a kind string for a declaration.
static QString decl_kind_string(const mx::Decl &decl) {
  switch (decl.category()) {
    case mx::DeclCategory::FUNCTION:
    case mx::DeclCategory::INSTANCE_METHOD:
    case mx::DeclCategory::CLASS_METHOD:
      return QStringLiteral("function");
    case mx::DeclCategory::CLASS:
    case mx::DeclCategory::STRUCTURE:
    case mx::DeclCategory::UNION:
    case mx::DeclCategory::ENUMERATION:
    case mx::DeclCategory::TYPE_ALIAS:
    case mx::DeclCategory::CONCEPT:
    case mx::DeclCategory::INTERFACE:
      return QStringLiteral("type");
    case mx::DeclCategory::LOCAL_VARIABLE:
    case mx::DeclCategory::GLOBAL_VARIABLE:
    case mx::DeclCategory::PARAMETER_VARIABLE:
    case mx::DeclCategory::INSTANCE_MEMBER:
    case mx::DeclCategory::CLASS_MEMBER:
      return QStringLiteral("variable");
    default:
      return QStringLiteral("other");
  }
}

// Get the first file path for a file entity.
static QString first_file_path(const mx::File &file) {
  for (auto path : file.paths()) {
    return QString::fromStdString(path.generic_string());
  }
  return {};
}

}  // namespace

// ===========================================================================
// SearchEntitiesTool
// ===========================================================================

QString SearchEntitiesTool::name(void) const {
  return QStringLiteral("search_entities");
}

QString SearchEntitiesTool::description(void) const {
  return QStringLiteral(
      "Search for code entities by name. Optionally filter by kind "
      "(function, type, variable).");
}

QJsonObject SearchEntitiesTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("query")] = string_prop(
      QStringLiteral("Entity name to search for"));
  props[QStringLiteral("kind")] = string_prop(
      QStringLiteral("Optional kind filter: function, type, variable"));
  return make_schema(props, {QStringLiteral("query")});
}

QJsonObject SearchEntitiesTool::execute(const QJsonObject &args) {
  QString query_str = args[QStringLiteral("query")].toString();
  if (query_str.isEmpty()) {
    return error_result(QStringLiteral("query is required"));
  }

  QString kind_filter = args[QStringLiteral("kind")].toString();

  const auto &index = m_ctx->config->Index();
  std::string query = query_str.toStdString();

  QJsonArray arr;
  int count = 0;
  static constexpr int kMaxResults = 100;

  for (mx::NamedEntity result : index.query_entities(query)) {
    if (count >= kMaxResults) {
      break;
    }

    if (std::holds_alternative<mx::NamedDecl>(result)) {
      mx::NamedDecl decl = std::get<mx::NamedDecl>(result);
      QString kind = decl_kind_string(decl);

      if (!kind_filter.isEmpty() && kind != kind_filter) {
        continue;
      }

      QJsonObject obj;
      obj[QStringLiteral("name")] =
          QString::fromStdString(std::string(decl.name()));
      obj[QStringLiteral("kind")] = kind;
      obj[QStringLiteral("entity_id")] =
          static_cast<qint64>(decl.id().Pack());
      arr.append(obj);
      ++count;

    } else if (std::holds_alternative<mx::DefineMacroDirective>(result)) {
      if (!kind_filter.isEmpty() && kind_filter != QLatin1String("macro")) {
        continue;
      }

      mx::DefineMacroDirective macro =
          std::get<mx::DefineMacroDirective>(result);
      QJsonObject obj;
      obj[QStringLiteral("name")] =
          QString::fromStdString(std::string(macro.name().data()));
      obj[QStringLiteral("kind")] = QStringLiteral("macro");
      obj[QStringLiteral("entity_id")] =
          static_cast<qint64>(macro.id().Pack());
      arr.append(obj);
      ++count;

    } else if (std::holds_alternative<mx::File>(result)) {
      if (!kind_filter.isEmpty() && kind_filter != QLatin1String("file")) {
        continue;
      }

      mx::File file = std::get<mx::File>(result);
      QString path = first_file_path(file);
      if (!path.isEmpty()) {
        QJsonObject obj;
        obj[QStringLiteral("name")] = path;
        obj[QStringLiteral("kind")] = QStringLiteral("file");
        obj[QStringLiteral("entity_id")] =
            static_cast<qint64>(file.id().Pack());
        arr.append(obj);
        ++count;
      }
    }
  }

  QJsonObject result;
  result[QStringLiteral("entities")] = arr;
  result[QStringLiteral("count")] = count;
  return result;
}

// ===========================================================================
// GetDefinitionTool
// ===========================================================================

QString GetDefinitionTool::name(void) const {
  return QStringLiteral("get_definition");
}

QString GetDefinitionTool::description(void) const {
  return QStringLiteral(
      "Get the source code for an entity by its ID.");
}

QJsonObject GetDefinitionTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("entity_id")] = int_prop(
      QStringLiteral("Entity ID (from search_entities)"));
  return make_schema(props, {QStringLiteral("entity_id")});
}

QJsonObject GetDefinitionTool::execute(const QJsonObject &args) {
  auto raw_id = static_cast<mx::RawEntityId>(
      args[QStringLiteral("entity_id")].toDouble(0));
  if (raw_id == 0) {
    return error_result(QStringLiteral("entity_id is required"));
  }

  const auto &index = m_ctx->config->Index();
  mx::VariantEntity vent = index.entity(mx::EntityId(raw_id));

  // Try to get tokens from the entity.
  QString code;
  QString file_path;
  int line = -1;

  auto extract_from_decl = [&](const mx::Decl &decl) {
    // Prefer the definition if available.
    std::optional<mx::Decl> def = decl.definition();
    const mx::Decl &target = def ? *def : decl;

    mx::TokenRange tokens = target.tokens();
    std::string token_text;
    for (mx::Token tok : tokens) {
      token_text += tok.data();
    }
    code = QString::fromStdString(token_text);

    // Get file and line from the first token.
    mx::Token first_tok = target.token();
    auto ft = first_tok.nearest_file_token();
    if (auto maybe_file = mx::File::containing(ft)) {
      file_path = first_file_path(*maybe_file);
    }
    auto loc = ft.nearest_location(m_ctx->config->FileLocationCache());
    if (loc) {
      line = static_cast<int>(loc->first);
    }
  };

  bool found = false;

  // Handle declaration entities.
  if (std::holds_alternative<mx::Decl>(vent)) {
    extract_from_decl(std::get<mx::Decl>(vent));
    found = true;
  }

  if (!found) {
    // Try via fragment.
    auto frag = index.fragment_containing(mx::EntityId(raw_id));
    if (frag) {
      mx::TokenRange tokens = frag->parsed_tokens();
      std::string token_text;
      for (mx::Token tok : tokens) {
        token_text += tok.data();
      }
      code = QString::fromStdString(token_text);
      found = true;
    }
  }

  if (!found) {
    return error_result(QStringLiteral("entity not found or not a declaration"));
  }

  QJsonObject result;
  result[QStringLiteral("code")] = code;
  if (!file_path.isEmpty()) {
    result[QStringLiteral("file")] = file_path;
  }
  if (line >= 0) {
    result[QStringLiteral("line")] = line;
  }
  return result;
}

// ===========================================================================
// GetReferencesTool
// ===========================================================================

QString GetReferencesTool::name(void) const {
  return QStringLiteral("get_references");
}

QString GetReferencesTool::description(void) const {
  return QStringLiteral(
      "Find references to an entity by its ID.");
}

QJsonObject GetReferencesTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("entity_id")] = int_prop(
      QStringLiteral("Entity ID (from search_entities)"));
  return make_schema(props, {QStringLiteral("entity_id")});
}

QJsonObject GetReferencesTool::execute(const QJsonObject &args) {
  auto raw_id = static_cast<mx::RawEntityId>(
      args[QStringLiteral("entity_id")].toDouble(0));
  if (raw_id == 0) {
    return error_result(QStringLiteral("entity_id is required"));
  }

  const auto &index = m_ctx->config->Index();
  mx::VariantEntity vent = index.entity(mx::EntityId(raw_id));

  if (std::holds_alternative<mx::NotAnEntity>(vent)) {
    return error_result(QStringLiteral("entity not found"));
  }

  QJsonArray arr;
  int count = 0;
  static constexpr int kMaxResults = 50;

  for (mx::Reference ref : mx::Reference::to(vent)) {
    if (count >= kMaxResults) {
      break;
    }

    QJsonObject obj;

    // Try to get location of the reference context.
    mx::VariantEntity context_entity = ref.context();

    if (std::holds_alternative<mx::Decl>(context_entity)) {
      const auto &ctx_decl = std::get<mx::Decl>(context_entity);
      mx::Token tok = ctx_decl.token();
      auto ft = tok.nearest_file_token();

      if (auto maybe_file = mx::File::containing(ft)) {
        obj[QStringLiteral("file")] = first_file_path(*maybe_file);
      }

      auto loc = ft.nearest_location(m_ctx->config->FileLocationCache());
      if (loc) {
        obj[QStringLiteral("line")] = static_cast<int>(loc->first);
      }

      // Brief context: first few tokens of the containing declaration.
      mx::TokenRange ctx_tokens = ctx_decl.tokens();
      std::string ctx_text;
      int tok_count = 0;
      for (mx::Token t : ctx_tokens) {
        ctx_text += t.data();
        if (++tok_count >= 20) {
          ctx_text += "...";
          break;
        }
      }
      obj[QStringLiteral("context")] = QString::fromStdString(ctx_text);
    }

    arr.append(obj);
    ++count;
  }

  QJsonObject result;
  result[QStringLiteral("references")] = arr;
  result[QStringLiteral("count")] = count;
  return result;
}

// ===========================================================================
// ListFilesTool
// ===========================================================================

QString ListFilesTool::name(void) const {
  return QStringLiteral("list_files");
}

QString ListFilesTool::description(void) const {
  return QStringLiteral("List all indexed source files.");
}

QJsonObject ListFilesTool::parametersSchema(void) const {
  return make_schema({}, {});
}

QJsonObject ListFilesTool::execute(const QJsonObject &) {
  const auto &index = m_ctx->config->Index();
  auto file_paths = index.file_paths();

  QJsonArray arr;
  for (const auto &[path, file_id] : file_paths) {
    QJsonObject obj;
    obj[QStringLiteral("path")] =
        QString::fromStdString(path.generic_string());
    obj[QStringLiteral("file_id")] =
        static_cast<qint64>(file_id.Pack());
    arr.append(obj);
  }

  QJsonObject result;
  result[QStringLiteral("files")] = arr;
  result[QStringLiteral("count")] = static_cast<int>(file_paths.size());
  return result;
}

// ===========================================================================
// GetDatabasePathTool
// ===========================================================================

QString GetDatabasePathTool::name(void) const {
  return QStringLiteral("get_database_path");
}

QString GetDatabasePathTool::description(void) const {
  return QStringLiteral(
      "Get the file path to the currently loaded multiplier database. "
      "Use this in Python scripts with Index.from_database().");
}

QJsonObject GetDatabasePathTool::parametersSchema(void) const {
  return make_schema({}, {});
}

QJsonObject GetDatabasePathTool::execute(const QJsonObject &) {
  QString path = m_ctx->config->DatabasePath();
  if (path.isEmpty()) {
    return error_result(QStringLiteral("no database loaded"));
  }

  QJsonObject result;
  result[QStringLiteral("path")] = path;
  return result;
}

// ===========================================================================
// Registration
// ===========================================================================

void registerNavigationTools(AgentToolRegistry &registry,
                             NavigationToolContext *ctx) {
  registry.registerTool(std::make_unique<SearchEntitiesTool>(ctx));
  registry.registerTool(std::make_unique<GetDefinitionTool>(ctx));
  registry.registerTool(std::make_unique<GetReferencesTool>(ctx));
  registry.registerTool(std::make_unique<ListFilesTool>(ctx));
  registry.registerTool(std::make_unique<GetDatabasePathTool>(ctx));
}

}  // namespace mx::gui
