// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "NavigationTools.h"

#include <QJsonArray>
#include <QJsonObject>

#include <multiplier/Frontend/File.h>
#include <multiplier/Frontend/Query.h>
#include <multiplier/Index.h>
#include <multiplier/Re2.h>
#include <multiplier/Reference.h>
#include <multiplier/GUI/Util.h>

#include <unordered_set>

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

static QJsonObject bool_prop(const QString &desc) {
  QJsonObject p;
  p[QStringLiteral("type")] = QStringLiteral("boolean");
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
      "Find references to an entity by its ID. Optionally filter by "
      "reference kind.");
}

QJsonObject GetReferencesTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("entity_id")] = int_prop(
      QStringLiteral("Entity ID (from search_entities)"));
  props[QStringLiteral("kind")] = string_prop(
      QStringLiteral("Filter by reference kind: calls, uses_value, uses_type, "
                     "writes, reads, takes_address, all (default: all)"));
  props[QStringLiteral("max_results")] = int_prop(
      QStringLiteral("Maximum number of results to return (default: 100)"));
  props[QStringLiteral("offset")] = int_prop(
      QStringLiteral("Skip this many matching results before returning (default: 0). "
                     "Use with max_results for pagination."));
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

  QString kind_filter = args[QStringLiteral("kind")].toString();
  if (kind_filter.isEmpty()) {
    kind_filter = QStringLiteral("all");
  }

  int max_results = static_cast<int>(
      args[QStringLiteral("max_results")].toDouble(100));
  if (max_results <= 0) {
    max_results = 100;
  }

  int offset = static_cast<int>(
      args[QStringLiteral("offset")].toDouble(0));
  if (offset < 0) {
    offset = 0;
  }

  // Build set of acceptable builtin reference kinds.
  auto matches_kind = [&](const mx::Reference &ref) -> bool {
    if (kind_filter == QLatin1String("all")) {
      return true;
    }
    auto brk = ref.builtin_reference_kind();
    if (!brk) {
      return kind_filter == QLatin1String("all");
    }
    auto k = *brk;
    if (kind_filter == QLatin1String("calls")) {
      return k == mx::BuiltinReferenceKind::CALLS;
    } else if (kind_filter == QLatin1String("uses_value")) {
      return k == mx::BuiltinReferenceKind::USES_VALUE;
    } else if (kind_filter == QLatin1String("uses_type")) {
      return k == mx::BuiltinReferenceKind::USES_TYPE;
    } else if (kind_filter == QLatin1String("writes")) {
      return k == mx::BuiltinReferenceKind::WRITES_VALUE ||
             k == mx::BuiltinReferenceKind::UPDATES_VALUE;
    } else if (kind_filter == QLatin1String("reads")) {
      return k == mx::BuiltinReferenceKind::USES_VALUE ||
             k == mx::BuiltinReferenceKind::ACCESSES_VALUE;
    } else if (kind_filter == QLatin1String("takes_address")) {
      return k == mx::BuiltinReferenceKind::TAKES_ADDRESS;
    }
    return true;
  };

  QJsonArray arr;
  int count = 0;      // Results returned.
  int matched = 0;    // Total matching (including skipped).
  bool has_more = false;

  for (mx::Reference ref : mx::Reference::to(vent)) {
    if (!matches_kind(ref)) {
      continue;
    }

    ++matched;

    // Skip until we've passed the offset.
    if (matched <= offset) {
      continue;
    }

    if (count >= max_results) {
      has_more = true;
      // Keep counting total matches for the summary.
      continue;
    }

    QJsonObject obj;

    // Include reference kind name.
    if (auto brk = ref.builtin_reference_kind()) {
      obj[QStringLiteral("ref_kind")] =
          QString::fromUtf8(mx::EnumeratorName(*brk));
    } else {
      obj[QStringLiteral("ref_kind")] =
          QString::fromStdString(std::string(ref.kind().data()));
    }

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
  result[QStringLiteral("total_matched")] = matched;
  result[QStringLiteral("offset")] = offset;
  result[QStringLiteral("has_more")] = has_more;
  return result;
}

// ===========================================================================
// GetCallersTool
// ===========================================================================

QString GetCallersTool::name(void) const {
  return QStringLiteral("get_callers");
}

QString GetCallersTool::description(void) const {
  return QStringLiteral(
      "Find all functions that call a given function. Returns the call "
      "hierarchy upward.");
}

QJsonObject GetCallersTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("entity_id")] = int_prop(
      QStringLiteral("Entity ID of the function to find callers of"));
  props[QStringLiteral("depth")] = int_prop(
      QStringLiteral("How many levels up to traverse (default 1)"));
  return make_schema(props, {QStringLiteral("entity_id")});
}

namespace {

struct CallerInfo {
  mx::RawEntityId entity_id;
  QString name;
  QString kind;
  QString file;
  int line;
};

static QJsonArray collect_callers(
    ConfigManager *config, const mx::Decl &target_decl,
    int depth, int max_depth,
    std::unordered_set<mx::RawEntityId> &visited) {

  QJsonArray callers_arr;

  // Normalize to canonical declaration.
  mx::Decl canonical = target_decl.canonical_declaration();

  std::unordered_set<mx::RawEntityId> seen_callers;

  for (mx::Reference ref : mx::Reference::to(canonical)) {
    auto brk = ref.builtin_reference_kind();
    if (!brk || *brk != mx::BuiltinReferenceKind::CALLS) {
      continue;
    }

    // Find the named entity containing the reference site.
    mx::VariantEntity use = ref.as_variant();
    mx::VariantEntity user = NamedEntityContaining(use);

    if (std::holds_alternative<mx::NotAnEntity>(user)) {
      continue;
    }

    // Normalize caller to canonical declaration.
    if (std::holds_alternative<mx::Decl>(user)) {
      user = std::get<mx::Decl>(user).canonical_declaration();
    }

    mx::RawEntityId caller_id = mx::EntityId(user).Pack();

    // Deduplicate.
    if (!seen_callers.insert(caller_id).second) {
      continue;
    }

    QJsonObject caller_obj;
    caller_obj[QStringLiteral("entity_id")] = static_cast<qint64>(caller_id);

    if (auto name_str = NameOfEntityAsString(user)) {
      caller_obj[QStringLiteral("name")] = *name_str;
    }

    if (std::holds_alternative<mx::Decl>(user)) {
      caller_obj[QStringLiteral("kind")] =
          decl_kind_string(std::get<mx::Decl>(user));
    }

    if (auto loc = LocationOfEntityEx(config->FileLocationCache(), user)) {
      caller_obj[QStringLiteral("file")] =
          QString::fromStdString(loc->path.generic_string());
      caller_obj[QStringLiteral("line")] = static_cast<int>(loc->line);
    }

    // Recurse if needed and not already visited in this traversal.
    if (depth < max_depth && std::holds_alternative<mx::Decl>(user) &&
        visited.insert(caller_id).second) {
      QJsonArray sub_callers = collect_callers(
          config, std::get<mx::Decl>(user), depth + 1, max_depth, visited);
      if (!sub_callers.isEmpty()) {
        caller_obj[QStringLiteral("callers")] = sub_callers;
      }
    }

    callers_arr.append(caller_obj);
  }

  return callers_arr;
}

}  // namespace

QJsonObject GetCallersTool::execute(const QJsonObject &args) {
  auto raw_id = static_cast<mx::RawEntityId>(
      args[QStringLiteral("entity_id")].toDouble(0));
  if (raw_id == 0) {
    return error_result(QStringLiteral("entity_id is required"));
  }

  int depth = static_cast<int>(args[QStringLiteral("depth")].toDouble(1));
  if (depth < 1) depth = 1;

  const auto &index = m_ctx->config->Index();
  mx::VariantEntity vent = index.entity(mx::EntityId(raw_id));

  if (std::holds_alternative<mx::NotAnEntity>(vent)) {
    return error_result(QStringLiteral("entity not found"));
  }

  // Resolve to a declaration.
  mx::VariantEntity containing = vent;
  if (!std::holds_alternative<mx::Decl>(containing)) {
    containing = NamedEntityContaining(vent);
  }

  if (!std::holds_alternative<mx::Decl>(containing)) {
    return error_result(QStringLiteral("entity is not a declaration"));
  }

  std::unordered_set<mx::RawEntityId> visited;
  QJsonArray callers = collect_callers(
      m_ctx->config, std::get<mx::Decl>(containing), 1, depth, visited);

  QJsonObject result;
  result[QStringLiteral("callers")] = callers;
  result[QStringLiteral("count")] = callers.size();
  return result;
}

// ===========================================================================
// GetCalleesTool
// ===========================================================================

QString GetCalleesTool::name(void) const {
  return QStringLiteral("get_callees");
}

QString GetCalleesTool::description(void) const {
  return QStringLiteral(
      "Find all functions called by a given function.");
}

QJsonObject GetCalleesTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("entity_id")] = int_prop(
      QStringLiteral("Entity ID of the function to find callees of"));
  props[QStringLiteral("depth")] = int_prop(
      QStringLiteral("How many levels down to traverse (default 1)"));
  return make_schema(props, {QStringLiteral("entity_id")});
}

namespace {

static QJsonArray collect_callees(
    ConfigManager *config, const mx::Decl &func_decl,
    int depth, int max_depth,
    std::unordered_set<mx::RawEntityId> &visited) {

  QJsonArray callees_arr;

  // Normalize to canonical declaration, then prefer the definition for
  // finding outgoing references (the definition has the function body).
  mx::Decl canonical = func_decl.canonical_declaration();
  std::optional<mx::Decl> def = canonical.definition();
  const mx::Decl &source = def ? *def : canonical;

  std::unordered_set<mx::RawEntityId> seen_callees;

  for (mx::Reference ref : mx::Reference::from(source)) {
    auto brk = ref.builtin_reference_kind();
    if (!brk || *brk != mx::BuiltinReferenceKind::CALLS) {
      continue;
    }

    // The referenced entity is the callee.
    mx::VariantEntity callee_vent = ref.as_variant();

    // Normalize to canonical declaration if it's a decl.
    if (std::holds_alternative<mx::Decl>(callee_vent)) {
      callee_vent = std::get<mx::Decl>(callee_vent).canonical_declaration();
    } else {
      // Try to find the named entity.
      callee_vent = NamedEntityContaining(callee_vent);
      if (std::holds_alternative<mx::Decl>(callee_vent)) {
        callee_vent = std::get<mx::Decl>(callee_vent).canonical_declaration();
      }
    }

    if (std::holds_alternative<mx::NotAnEntity>(callee_vent)) {
      continue;
    }

    mx::RawEntityId callee_id = mx::EntityId(callee_vent).Pack();

    // Deduplicate.
    if (!seen_callees.insert(callee_id).second) {
      continue;
    }

    QJsonObject callee_obj;
    callee_obj[QStringLiteral("entity_id")] = static_cast<qint64>(callee_id);

    if (auto name_str = NameOfEntityAsString(callee_vent)) {
      callee_obj[QStringLiteral("name")] = *name_str;
    }

    if (std::holds_alternative<mx::Decl>(callee_vent)) {
      callee_obj[QStringLiteral("kind")] =
          decl_kind_string(std::get<mx::Decl>(callee_vent));
    }

    if (auto loc = LocationOfEntityEx(config->FileLocationCache(),
                                      callee_vent)) {
      callee_obj[QStringLiteral("file")] =
          QString::fromStdString(loc->path.generic_string());
      callee_obj[QStringLiteral("line")] = static_cast<int>(loc->line);
    }

    // Recurse if needed.
    if (depth < max_depth && std::holds_alternative<mx::Decl>(callee_vent) &&
        visited.insert(callee_id).second) {
      QJsonArray sub_callees = collect_callees(
          config, std::get<mx::Decl>(callee_vent),
          depth + 1, max_depth, visited);
      if (!sub_callees.isEmpty()) {
        callee_obj[QStringLiteral("callees")] = sub_callees;
      }
    }

    callees_arr.append(callee_obj);
  }

  return callees_arr;
}

}  // namespace

QJsonObject GetCalleesTool::execute(const QJsonObject &args) {
  auto raw_id = static_cast<mx::RawEntityId>(
      args[QStringLiteral("entity_id")].toDouble(0));
  if (raw_id == 0) {
    return error_result(QStringLiteral("entity_id is required"));
  }

  int depth = static_cast<int>(args[QStringLiteral("depth")].toDouble(1));
  if (depth < 1) depth = 1;

  const auto &index = m_ctx->config->Index();
  mx::VariantEntity vent = index.entity(mx::EntityId(raw_id));

  if (std::holds_alternative<mx::NotAnEntity>(vent)) {
    return error_result(QStringLiteral("entity not found"));
  }

  // Resolve to a declaration.
  mx::VariantEntity containing = vent;
  if (!std::holds_alternative<mx::Decl>(containing)) {
    containing = NamedEntityContaining(vent);
  }

  if (!std::holds_alternative<mx::Decl>(containing)) {
    return error_result(QStringLiteral("entity is not a declaration"));
  }

  std::unordered_set<mx::RawEntityId> visited;
  QJsonArray callees = collect_callees(
      m_ctx->config, std::get<mx::Decl>(containing), 1, depth, visited);

  QJsonObject result;
  result[QStringLiteral("callees")] = callees;
  result[QStringLiteral("count")] = callees.size();
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
// SearchCodeTool
// ===========================================================================

QString SearchCodeTool::name(void) const {
  return QStringLiteral("search_code");
}

QString SearchCodeTool::description(void) const {
  return QStringLiteral(
      "Search all indexed source code for a regex pattern. Returns matching "
      "lines with file, line number, and context.");
}

QJsonObject SearchCodeTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("pattern")] = string_prop(
      QStringLiteral("Regex pattern to search for (RE2 syntax)"));
  props[QStringLiteral("case_sensitive")] = bool_prop(
      QStringLiteral("Whether the search is case sensitive (default: false)"));
  props[QStringLiteral("max_results")] = int_prop(
      QStringLiteral("Maximum number of results to return (default: 100)"));
  props[QStringLiteral("offset")] = int_prop(
      QStringLiteral("Skip this many matches before returning (default: 0). "
                     "Use with max_results for pagination."));
  return make_schema(props, {QStringLiteral("pattern")});
}

QJsonObject SearchCodeTool::execute(const QJsonObject &args) {
  QString pattern_str = args[QStringLiteral("pattern")].toString();
  if (pattern_str.isEmpty()) {
    return error_result(QStringLiteral("pattern is required"));
  }

  bool case_sensitive = args[QStringLiteral("case_sensitive")].toBool(false);

  int max_results = static_cast<int>(
      args[QStringLiteral("max_results")].toDouble(100));
  if (max_results <= 0) {
    max_results = 100;
  }

  int offset = static_cast<int>(
      args[QStringLiteral("offset")].toDouble(0));
  if (offset < 0) {
    offset = 0;
  }

  // Build the RE2 pattern. Prepend (?i) for case-insensitive matching.
  std::string re2_pattern;
  if (!case_sensitive) {
    re2_pattern = "(?i)";
  }
  re2_pattern += pattern_str.toStdString();

  mx::RegexQuery query(std::move(re2_pattern));
  if (!query.is_valid()) {
    return error_result(QStringLiteral("invalid regex pattern"));
  }

  const auto &index = m_ctx->config->Index();

  QJsonArray arr;
  int count = 0;
  int total_matched = 0;
  bool has_more = false;

  for (mx::File file : index.files()) {
    // Get a display path for the file.
    QString file_path;
    mx::RawEntityId file_entity_id = file.id().Pack();
    for (auto path : file.paths()) {
      file_path = QString::fromStdString(path.generic_string());
      break;
    }

    for (mx::RegexQueryMatch match : query.match_fragments(file)) {
      ++total_matched;

      // Skip until we've passed the offset.
      if (total_matched <= offset) {
        continue;
      }

      if (count >= max_results) {
        has_more = true;
        continue;
      }

      std::string_view match_data = match.data();
      QString match_text = QString::fromUtf8(
          match_data.data(), static_cast<qsizetype>(match_data.size()));

      // Get the line number from the first token of the match.
      int line = -1;
      for (mx::Token tok : match) {
        mx::Token ft = tok.file_token();
        if (!ft) {
          ft = tok;
        }
        auto loc = ft.nearest_location(m_ctx->config->FileLocationCache());
        if (loc) {
          line = static_cast<int>(loc->first);
        }
        break;
      }

      // Build a context snippet: the full line(s) containing the match.
      // The match data points into the file's token data, so we extract
      // surrounding context by finding line boundaries.
      QString context = match_text;
      if (!match_data.empty()) {
        // Walk backwards to find the line start.
        const char *data_ptr = match_data.data();
        const char *line_start = data_ptr;

        // Walk forward to find the line end.
        const char *line_end = data_ptr + match_data.size();

        // Trim to just the first line of a multi-line match for context.
        const char *first_newline = data_ptr;
        while (first_newline < line_end && *first_newline != '\n') {
          ++first_newline;
        }

        // Use the match text up to the first newline as context.
        if (first_newline < line_end) {
          context = QString::fromUtf8(
              line_start,
              static_cast<qsizetype>(first_newline - line_start));
        }
      }

      QJsonObject obj;
      obj[QStringLiteral("file")] = file_path;
      if (line >= 0) {
        obj[QStringLiteral("line")] = line;
      }
      obj[QStringLiteral("match")] = match_text;
      obj[QStringLiteral("context")] = context;
      obj[QStringLiteral("entity_id")] = static_cast<qint64>(file_entity_id);
      arr.append(obj);
      ++count;
    }
  }

  QJsonObject result;
  result[QStringLiteral("matches")] = arr;
  result[QStringLiteral("count")] = count;
  result[QStringLiteral("total_matched")] = total_matched;
  result[QStringLiteral("offset")] = offset;
  result[QStringLiteral("has_more")] = has_more;
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
  registry.registerTool(std::make_unique<GetCallersTool>(ctx));
  registry.registerTool(std::make_unique<GetCalleesTool>(ctx));
  registry.registerTool(std::make_unique<ListFilesTool>(ctx));
  registry.registerTool(std::make_unique<GetDatabasePathTool>(ctx));
  registry.registerTool(std::make_unique<SearchCodeTool>(ctx));
}

}  // namespace mx::gui
