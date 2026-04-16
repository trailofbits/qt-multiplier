// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "PythonTools.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTemporaryFile>

namespace mx::gui {
namespace {

static int s_next_script_id = 1;

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

static QJsonObject int_prop(const QString &desc, int default_value) {
  QJsonObject p;
  p[QStringLiteral("type")] = QStringLiteral("integer");
  p[QStringLiteral("description")] = desc;
  p[QStringLiteral("default")] = default_value;
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

// Set up venv environment variables on the process if the interpreter lives
// inside a virtual environment.
static void setup_venv_environment(QProcess &proc, const QString &interp) {
  QFileInfo fi(interp);
  auto bin_dir = fi.absolutePath();
  auto venv_dir = QFileInfo(bin_dir).absolutePath();
  auto pyvenv_cfg = venv_dir + QStringLiteral("/pyvenv.cfg");
  if (QFileInfo::exists(pyvenv_cfg)) {
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("VIRTUAL_ENV"), venv_dir);
    env.insert(QStringLiteral("PATH"),
               bin_dir + QStringLiteral(":") +
                   env.value(QStringLiteral("PATH")));
    proc.setProcessEnvironment(env);
  }
}

static QString scripts_directory(void) {
  auto base = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
  return base + QStringLiteral("/multiplier_scripts");
}

}  // namespace

// ===========================================================================
// RunPythonTool
// ===========================================================================

QString RunPythonTool::name(void) const {
  return QStringLiteral("run_python");
}

QString RunPythonTool::description(void) const {
  return QStringLiteral(
      "Execute Python code and return stdout/stderr. The multiplier Python "
      "bindings are available for programmatic analysis.");
}

QJsonObject RunPythonTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("code")] =
      string_prop(QStringLiteral("Python code to execute"));
  props[QStringLiteral("timeout_seconds")] =
      int_prop(QStringLiteral("Execution timeout in seconds"), 30);
  return make_schema(props, {QStringLiteral("code")});
}

QJsonObject RunPythonTool::execute(const QJsonObject &args) {
  QString code = args[QStringLiteral("code")].toString();
  if (code.isEmpty()) {
    return error_result(QStringLiteral("code is required"));
  }

  int timeout_seconds = args[QStringLiteral("timeout_seconds")].toInt(30);
  if (timeout_seconds <= 0) {
    timeout_seconds = 30;
  }

  // Determine interpreter path.
  QString interp = m_ctx->config->PythonInterpreterPath();
  if (interp.isEmpty()) {
    interp = QStringLiteral("python3");
  }

  // Write code to a temporary file.
  QTemporaryFile tmp_file;
  tmp_file.setFileTemplate(
      QDir::tempPath() + QStringLiteral("/mx_agent_XXXXXX.py"));
  if (!tmp_file.open()) {
    return error_result(QStringLiteral("Failed to create temporary file"));
  }
  tmp_file.write(code.toUtf8());
  tmp_file.flush();

  // Run the interpreter.
  QProcess proc;
  setup_venv_environment(proc, interp);
  proc.setProgram(interp);
  proc.setArguments({tmp_file.fileName()});
  proc.start();

  if (!proc.waitForStarted(5000)) {
    return error_result(
        QStringLiteral("Failed to start Python interpreter: ") + interp);
  }

  bool finished = proc.waitForFinished(timeout_seconds * 1000);

  QString stdout_str =
      QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
  QString stderr_str =
      QString::fromUtf8(proc.readAllStandardError()).trimmed();

  QJsonObject result;
  result[QStringLiteral("stdout")] = stdout_str;
  result[QStringLiteral("stderr")] = stderr_str;
  result[QStringLiteral("exit_code")] = proc.exitCode();
  result[QStringLiteral("timed_out")] = !finished;

  if (!finished) {
    proc.kill();
    proc.waitForFinished(2000);
  }

  return result;
}

// ===========================================================================
// CreateScriptFileTool
// ===========================================================================

QString CreateScriptFileTool::name(void) const {
  return QStringLiteral("create_script_file");
}

QString CreateScriptFileTool::description(void) const {
  return QStringLiteral(
      "Write Python code to a persistent script file and return the path. "
      "Useful for creating reusable scripts.");
}

QJsonObject CreateScriptFileTool::parametersSchema(void) const {
  QJsonObject props;
  props[QStringLiteral("code")] =
      string_prop(QStringLiteral("Python code to write"));
  props[QStringLiteral("filename")] =
      string_prop(QStringLiteral("Optional filename (default: auto-generated)"));
  return make_schema(props, {QStringLiteral("code")});
}

QJsonObject CreateScriptFileTool::execute(const QJsonObject &args) {
  QString code = args[QStringLiteral("code")].toString();
  if (code.isEmpty()) {
    return error_result(QStringLiteral("code is required"));
  }

  QString filename = args[QStringLiteral("filename")].toString();
  if (filename.isEmpty()) {
    filename = QStringLiteral("agent_script_%1.py").arg(s_next_script_id++);
  }

  // Ensure the scripts directory exists.
  QString dir_path = scripts_directory();
  QDir dir(dir_path);
  if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
    return error_result(
        QStringLiteral("Failed to create scripts directory: ") + dir_path);
  }

  QString file_path = dir_path + QStringLiteral("/") + filename;
  QFile file(file_path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    return error_result(
        QStringLiteral("Failed to write file: ") + file_path);
  }
  file.write(code.toUtf8());
  file.close();

  QJsonObject result;
  result[QStringLiteral("path")] = file_path;
  result[QStringLiteral("filename")] = filename;
  return result;
}

// ===========================================================================
// GetPythonApiReferenceTool
// ===========================================================================

QString GetPythonApiReferenceTool::name(void) const {
  return QStringLiteral("get_python_api_reference");
}

QString GetPythonApiReferenceTool::description(void) const {
  return QStringLiteral(
      "Get a reference guide for the multiplier Python API, including common "
      "patterns and examples.");
}

QJsonObject GetPythonApiReferenceTool::parametersSchema(void) const {
  return make_schema({}, {});
}

QJsonObject GetPythonApiReferenceTool::execute(const QJsonObject &) {
  QJsonObject result;

  result[QStringLiteral("overview")] = QStringLiteral(
      "The multiplier Python bindings provide programmatic access to the "
      "indexed codebase.");

  result[QStringLiteral("setup")] = QStringLiteral(
      "from multiplier import Index\n"
      "idx = Index.in_memory_cache(Index.from_database('/path/to/file.db'))");

  result[QStringLiteral("note")] = QStringLiteral(
      "The database path should match the currently loaded project. Use "
      "search_entities or list_files to find entities, then use Python for "
      "bulk analysis.");

  // Common patterns.
  QJsonArray patterns;

  auto add_pattern = [&](const QString &name, const QString &code) {
    QJsonObject p;
    p[QStringLiteral("name")] = name;
    p[QStringLiteral("code")] = code;
    patterns.append(p);
  };

  add_pattern(
      QStringLiteral("List all files"),
      QStringLiteral("for path in idx.file_paths():\n    print(path)"));

  add_pattern(
      QStringLiteral("Search for entities by name"),
      QStringLiteral(
          "for entity in idx.query_entities('function_name'):\n"
          "    print(type(entity).__name__, entity)"));

  add_pattern(
      QStringLiteral("Get a function's source code"),
      QStringLiteral(
          "from multiplier import Decl, NamedDecl\n"
          "for entity in idx.query_entities('my_func'):\n"
          "    if isinstance(entity, NamedDecl):\n"
          "        decl = Decl.from_(entity)\n"
          "        for tok in decl.tokens():\n"
          "            print(tok.data(), end='')"));

  add_pattern(
      QStringLiteral("Find all references to an entity"),
      QStringLiteral(
          "from multiplier import Reference\n"
          "entity = idx.entity(entity_id)\n"
          "for ref in Reference.to(entity):\n"
          "    ctx = ref.context()\n"
          "    print(ctx)"));

  add_pattern(
      QStringLiteral("Get file containing an entity"),
      QStringLiteral(
          "from multiplier import File\n"
          "f = File.containing(entity)\n"
          "if f:\n"
          "    for path in f.paths():\n"
          "        print(path)"));

  add_pattern(
      QStringLiteral("Iterate all functions in the index"),
      QStringLiteral(
          "from multiplier import Decl, FunctionDecl\n"
          "for decl in idx.declarations():\n"
          "    if isinstance(decl, FunctionDecl):\n"
          "        print(decl.name())"));

  add_pattern(
      QStringLiteral("Get callers of a function"),
      QStringLiteral(
          "from multiplier import Reference\n"
          "for ref in Reference.to(func_decl):\n"
          "    caller = ref.context()\n"
          "    if isinstance(caller, Decl):\n"
          "        print('Called from:', caller)"));

  result[QStringLiteral("common_patterns")] = patterns;

  // Key types.
  QJsonArray types;
  types.append(QStringLiteral(
      "Index - The main entry point. Open with Index.from_database() or "
      "Index.in_memory_cache()"));
  types.append(QStringLiteral(
      "File - A source file. Get via File.containing(entity) or iterate "
      "idx.files()"));
  types.append(QStringLiteral(
      "Fragment - A code fragment (translation unit piece). Contains "
      "declarations"));
  types.append(QStringLiteral(
      "Decl - A declaration (function, variable, type, etc.)"));
  types.append(QStringLiteral(
      "NamedDecl - A declaration with a name. Subclass of Decl"));
  types.append(QStringLiteral("FunctionDecl - A function declaration"));
  types.append(QStringLiteral(
      "Token - A single token with data(), kind(), category(), "
      "related_entity()"));
  types.append(QStringLiteral(
      "TokenRange - A range of tokens. Iterate with for tok in range"));
  types.append(QStringLiteral(
      "Reference - A reference from one entity to another. Find with "
      "Reference.to(entity)"));
  types.append(QStringLiteral(
      "EntityId - Packed entity identifier. Get with entity.id().pack()"));
  result[QStringLiteral("key_types")] = types;

  // Tips.
  QJsonArray tips;
  tips.append(QStringLiteral(
      "Always use Index.in_memory_cache() to wrap Index.from_database() for "
      "better performance"));
  tips.append(QStringLiteral(
      "Entity IDs are stable within a database - use them for "
      "cross-referencing"));
  tips.append(QStringLiteral("Token.data() returns the raw text of a token"));
  tips.append(QStringLiteral(
      "Decl.canonical_declaration() returns the canonical version across "
      "redeclarations"));
  tips.append(QStringLiteral(
      "Use Decl.definition() to prefer definitions over forward "
      "declarations"));
  result[QStringLiteral("tips")] = tips;

  return result;
}

// ===========================================================================
// Registration
// ===========================================================================

void registerPythonTools(AgentToolRegistry &registry, PythonToolContext *ctx) {
  registry.registerTool(std::make_unique<RunPythonTool>(ctx));
  registry.registerTool(std::make_unique<CreateScriptFileTool>(ctx));
  registry.registerTool(std::make_unique<GetPythonApiReferenceTool>(ctx));
}

}  // namespace mx::gui
