# Multiplier Python Bindings

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
- `.IN()`, `.containing()`, `.callers` return generators — consume or list().
