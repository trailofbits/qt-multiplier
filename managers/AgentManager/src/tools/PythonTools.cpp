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
#include <QMetaObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTemporaryFile>

#include <multiplier/GUI/Managers/ConfigManager.h>

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

static QString workspace_base(ConfigManager *config) {
  auto ws = config->WorkspacePath();
  if (!ws.isEmpty()) {
    return ws;
  }
  return QStandardPaths::writableLocation(QStandardPaths::TempLocation)
         + QStringLiteral("/multiplier_workspace");
}

static QString scripts_directory(ConfigManager *config) {
  return workspace_base(config) + QStringLiteral("/scripts");
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

  // Expose the current database path and workspace directory so scripts can
  // find them without calling tools first.
  {
    auto env = proc.processEnvironment();
    if (env.isEmpty()) {
      env = QProcessEnvironment::systemEnvironment();
    }
    auto db_path = m_ctx->config->DatabasePath();
    if (!db_path.isEmpty()) {
      env.insert(QStringLiteral("MULTIPLIER_DATABASE"), db_path);
    }
    env.insert(QStringLiteral("MULTIPLIER_WORKSPACE"),
               workspace_base(m_ctx->config));
    proc.setProcessEnvironment(env);
  }

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
  QString dir_path = scripts_directory(m_ctx->config);
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
// GetWorkspacePathTool
// ===========================================================================

QString GetWorkspacePathTool::name(void) const {
  return QStringLiteral("get_workspace_path");
}

QString GetWorkspacePathTool::description(void) const {
  return QStringLiteral(
      "Get the configured workspace directory for saving artifacts like "
      "scripts, reports, and fuzzer harnesses. Returns the temp directory "
      "if no workspace is configured.");
}

QJsonObject GetWorkspacePathTool::parametersSchema(void) const {
  return make_schema({}, {});
}

QJsonObject GetWorkspacePathTool::execute(const QJsonObject &) {
  auto ws = workspace_base(m_ctx->config);

  // Ensure the directory exists.
  QDir dir(ws);
  if (!dir.exists()) {
    dir.mkpath(QStringLiteral("."));
  }

  QJsonObject result;
  result[QStringLiteral("workspace_path")] = ws;
  result[QStringLiteral("is_configured")] =
      !m_ctx->config->WorkspacePath().isEmpty();
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

static const QString kApiRefTitle =
    QStringLiteral("Multiplier Python API Reference");

static const QString kApiRefContent = QString::fromUtf8(
R"MX(# Multiplier Python Bindings

Write Python scripts using the Multiplier binary analysis framework's Python bindings. Multiplier indexes C/C++ codebases into a database, and the Python API lets you query entities (functions, types, variables, macros), navigate the AST, extract source code, find references, and trace call graphs.

## Opening the Database

Always wrap with `in_memory_cache` for performance:

```python
import os
import multiplier as mx

db_path = os.environ['MULTIPLIER_DATABASE']
index = mx.Index.in_memory_cache(mx.Index.from_database(db_path))
```

Never call `Index.from_database()` without wrapping it in `Index.in_memory_cache()`.

## Iterating Entities

Use `.IN(scope)` to iterate entities of a specific type:

```python
# All functions
seen = set()
for func in mx.ast.FunctionDecl.IN(index):
    func = func.canonical_declaration
    if func.id in seen or func.is_implicit:
        continue
    seen.add(func.id)
    print(func.name)

# All variables
for var in mx.ast.VarDecl.IN(index):
    print(var.name)

# All macros
for macro in mx.frontend.DefineMacroDirective.IN(index):
    print(macro.name.data)

# Functions in a specific file
for func in mx.ast.FunctionDecl.IN(file_obj):
    print(func.name)
```

Always deduplicate with `canonical_declaration` and a seen set.

## Extracting Source Code

Use Fragment and TokenTree for macro-aware source:

```python
fragment = mx.Fragment.containing(decl)
if fragment:
    tree = mx.frontend.TokenTree.create(fragment)
    tokens = tree.serialize(mx.frontend.TokenTreeVisitor())
    source = "".join(tok.data for tok in tokens)
```

For just the declaration's tokens: `"".join(tok.data for tok in decl.tokens)`

## Token Properties

```python
for tok in decl.tokens:
    tok.data           # The text
    tok.kind           # TokenKind enum
    tok.category       # TokenCategory
    tok.related_entity # AST entity this token refers to

    # File location
    ft = tok.nearest_file_token
    if ft:
        flc = mx.frontend.FileLocationCache()
        loc = ft.nearest_location(flc)  # (line, col)
```

## Finding References

```python
# Who references this entity?
for ref in mx.Reference.to(decl):
    brk = ref.builtin_reference_kind
    if brk == mx.BuiltinReferenceKind.CALLS:
        # This is a call site
        caller = ref.context

# What does this entity reference?
for ref in mx.Reference.FROM(decl):
    pass
```

## Call Graph

```python
# Callers (FunctionDecl-specific)
for call_stmt in func.callers:
    caller = next(iter(mx.ast.FunctionDecl.containing(call_stmt)), None)
    if caller:
        print(f"{caller.canonical_declaration.name} calls {func.name}")

# Via references (more general)
for ref in mx.Reference.to(func_decl):
    if ref.builtin_reference_kind == mx.BuiltinReferenceKind.CALLS:
        for caller in mx.ast.FunctionDecl.containing(ref.context):
            print(f"Called by: {caller.canonical_declaration.name}")
```

## Type Conversions

Use `.FROM()` for safe downcasting (returns None if wrong type):

```python
func = mx.ast.FunctionDecl.FROM(generic_decl)
if func is not None:
    print(f"Function: {func.name}")

record = mx.ast.RecordDecl.FROM(decl)
if record:
    for field in record.fields:
        print(f"  field: {field.name}")
```

## File and Location

```python
file = mx.frontend.File.containing(decl)
if file:
    for path in file.paths:
        print(path)

for file in index.files:
    for path in file.paths:
        print(path)
```

## Common Types

- `mx.ast.FunctionDecl` - functions/methods
- `mx.ast.VarDecl` - variables
- `mx.ast.ParmVarDecl` - function parameters
- `mx.ast.FieldDecl` - struct/class fields
- `mx.ast.RecordDecl` / `mx.ast.CXXRecordDecl` - struct/class/union
- `mx.ast.EnumDecl` / `mx.ast.EnumConstantDecl` - enums
- `mx.ast.TypedefDecl` - typedefs
- `mx.ast.NamedDecl` - base for named entities

## Reference Kinds

CALLS, USES_VALUE, USES_TYPE, WRITES_VALUE, UPDATES_VALUE, TAKES_ADDRESS, TAKES_VALUE, ACCESSES_VALUE, CASTS_WITH_TYPE, INCLUDES_FILE, EXPANSION_OF, EXTENDS, OVERRIDES, SPECIALIZES

## Example: Find Functions Handling User Input

```python
import os, multiplier as mx

index = mx.Index.in_memory_cache(
    mx.Index.from_database(os.environ['MULTIPLIER_DATABASE']))
flc = mx.frontend.FileLocationCache()

input_funcs = {'read', 'recv', 'fgets', 'scanf', 'getline', 'fread'}
seen = set()
for func in mx.ast.FunctionDecl.IN(index):
    func = func.canonical_declaration
    if func.id in seen or func.is_implicit or func.name not in input_funcs:
        continue
    seen.add(func.id)
    for ref in mx.Reference.to(func):
        if ref.builtin_reference_kind != mx.BuiltinReferenceKind.CALLS:
            continue
        for caller in mx.ast.FunctionDecl.containing(ref.context):
            caller = caller.canonical_declaration
            file = mx.frontend.File.containing(caller)
            path = next(iter(file.paths), "?") if file else "?"
            print(f"{caller.name} calls {func.name} in {path}")
```

## Tips

- Always `Index.in_memory_cache()`. Without it, every query hits disk.
- Always `canonical_declaration` + seen set for deduplication.
- Filter `is_implicit` to skip compiler-generated declarations.
- Use `Fragment.containing()` + `TokenTree` for macro-aware source.
- Wrap entity operations in try/except for safety.
- `.IN()`, `.containing()`, `.callers` return generators — consume or list().)MX");

// Find or create the API reference document.
static QString ensureApiRefDocument(ConfigManager *config) {
  QVector<ConfigManager::DocumentInfo> docs;
  QMetaObject::invokeMethod(config, [&] {
    docs = config->LoadDocumentsByCategory(QStringLiteral("reference"));
  }, Qt::BlockingQueuedConnection);

  for (const auto &doc : docs) {
    if (doc.title == kApiRefTitle) {
      QString content;
      QMetaObject::invokeMethod(config, [&] {
        content = config->LoadDocumentContent(doc.doc_id);
      }, Qt::BlockingQueuedConnection);
      return content;
    }
  }

  // Create it.
  int doc_id = -1;
  QMetaObject::invokeMethod(config, [&] {
    doc_id = config->CreateDocument(kApiRefContent, kApiRefTitle);
    if (doc_id >= 0) {
      config->SetDocumentCategory(doc_id, QStringLiteral("reference"));
    }
  }, Qt::BlockingQueuedConnection);
  return kApiRefContent;
}

QJsonObject GetPythonApiReferenceTool::execute(const QJsonObject &) {
  auto content = ensureApiRefDocument(m_ctx->config);

  QJsonObject result;
  result[QStringLiteral("reference")] = content;
  result[QStringLiteral("note")] = QStringLiteral(
      "This reference is also stored as a document titled '"
      "Multiplier Python API Reference' (category: reference). "
      "You can edit it with edit_document.");
  return result;
}

// ===========================================================================
// Registration
// ===========================================================================

void registerPythonTools(AgentToolRegistry &registry, PythonToolContext *ctx) {
  registry.registerTool(std::make_unique<RunPythonTool>(ctx));
  registry.registerTool(std::make_unique<CreateScriptFileTool>(ctx));
  registry.registerTool(std::make_unique<GetWorkspacePathTool>(ctx));
  registry.registerTool(std::make_unique<GetPythonApiReferenceTool>(ctx));
}

}  // namespace mx::gui
