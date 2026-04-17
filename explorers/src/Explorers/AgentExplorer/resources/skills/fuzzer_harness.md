# Writing Fuzzer Harnesses

Guide for writing effective fuzz harnesses that exercise real code paths with minimal dependencies.

## Harness Design Philosophy

A good harness is minimal: it pulls in exactly the code under test and nothing more. The goal is to get the target function executing against fuzzer-generated inputs as fast as possible, with as few dependencies as possible.

### Dependency Minimization

When writing a harness for a function in a large codebase:

1. **Start with the function signature.** What types does it take? What headers?
2. **Use Multiplier references** to find what the function actually calls. Get callees with `get_callees` — these are your true dependencies.
3. **Stub what you can.** If a callee does logging, config lookup, or other non-essential work, stub it out with a no-op or a simple return value. The fuzzer doesn't need logging.
4. **Copy what you must.** For core logic functions that the target actually depends on, copy them into the harness file directly. This eliminates build system complexity.
5. **Rewrite selectively.** If a dependency pulls in a massive header chain, consider rewriting the relevant struct/typedef inline. A 5-line struct definition beats a 500-header include chain.

```c
// Instead of #include "massive_internal_header.h", just define what you need:
typedef struct {
    uint8_t *data;
    size_t len;
    int type;
} message_t;
```

### What NOT to stub
- Memory allocation (use the real allocator — bugs hide there)
- Bounds checking functions (that's what you're testing)
- Type conversion functions (integer overflow lives here)
- The actual parsing/processing logic

### What to stub
- Logging/debug output
- Configuration/settings lookup (hardcode sensible defaults)
- Network I/O (replace with buffer reads from fuzzer input)
- File I/O (replace with in-memory buffers)
- Authentication/authorization checks (bypass or always-succeed)

## Harness Structure

### LibFuzzer harness (C/C++)
```c
#include <stdint.h>
#include <stddef.h>

// Minimal includes from the target
#include "target_types.h"  // or inline the types

// Stubs for non-essential dependencies
void log_message(int level, const char *msg) { (void)level; (void)msg; }

// The function under test (declared, linked from target object file,
// or copy-pasted if small enough)
extern int parse_message(const uint8_t *data, size_t len);

int LLVMFuzzerTestOneHarness(const uint8_t *data, size_t size) {
    if (size < 4) return 0;  // Minimum meaningful input
    parse_message(data, size);
    return 0;
}
```

### AFL harness
```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

extern int parse_message(const uint8_t *data, size_t len);

int main(void) {
    uint8_t buf[1024 * 64];
    ssize_t n = read(0, buf, sizeof(buf));
    if (n > 0) {
        parse_message(buf, (size_t)n);
    }
    return 0;
}
```

## Using Multiplier to Build Harnesses

### Finding the target
```
search_entities("parse_message", kind="function")
→ result_id: "r-1", entity_id: "12345"
```

### Getting the signature
```
get_definition(entity_id="12345", follows="r-1.0")
→ result_id: "r-2", int parse_message(const uint8_t *data, size_t len, msg_context_t *ctx)
```

### Finding dependencies
```
get_callees(entity_id="12345", depth=2, follows="r-2")
→ result_id: "r-3", Tree of functions called by parse_message
```

Use `follows` to declare provenance: each step references the result that motivated it.

### Deciding what to include vs stub
For each callee, ask:
- Is it essential to the logic being tested? → Include
- Is it a side effect (logging, stats, notification)? → Stub
- Does it have a huge dependency chain? → Copy the function, stub its dependencies

### Getting type definitions
```
search_entities("msg_context_t", kind="type")
get_definition(entity_id="67890")
→ Full struct definition with fields
```

Copy the struct definition into the harness. If it references other types, recurse — but stop at basic types (int, char *, size_t).

## Compilation

The harness needs to compile against the target code. Key considerations:

### Compiler flags
- Use `-fsanitize=fuzzer,address,undefined` for LibFuzzer + sanitizers
- Use `-g` for debug symbols (crash triage needs them)
- Use `-O1` or `-O2` (not `-O0` — too slow for fuzzing, not `-O3` — can optimize away bugs)

### Environment variables

The following are automatically set in Python scripts from the agent configuration:
- `CC` — C compiler path (e.g. `/usr/bin/cc`)
- `CXX` — C++ compiler path (e.g. `/usr/bin/c++`)
- `SDKROOT` — macOS SDK root path
- `MULTIPLIER_WORKSPACE` — workspace directory for saving harness files

Use them in compilation scripts:
```python
import os, subprocess
cc = os.environ.get('CC', 'cc')
workspace = os.environ['MULTIPLIER_WORKSPACE']
harness_dir = os.path.join(workspace, 'harnesses')
os.makedirs(harness_dir, exist_ok=True)

subprocess.run([cc, '-fsanitize=fuzzer,address', '-O1', '-g',
                '-o', f'{harness_dir}/fuzz_parser', 'harness.c', 'target.c'])
```

### macOS specifics
On macOS, `SDKROOT` is set from the agent configuration. If not configured, detect it:
```python
import subprocess
sdk = os.environ.get('SDKROOT') or subprocess.check_output(
    ['xcrun', '--show-sdk-path']).decode().strip()
```

### Linking
- If the target is a library: link against it
- If copying source: compile the copied functions alongside the harness
- Avoid pulling in the entire build system — compile just what you need
- Save compiled harnesses to `$MULTIPLIER_WORKSPACE/harnesses/`

## Seed Corpus

Start the fuzzer with meaningful inputs, not random bytes:
- Capture real protocol messages or file samples
- Generate edge cases: empty input, maximum size, all-zeros, all-0xFF
- Include inputs that trigger different code paths (different message types, versions)

## Triage

When the fuzzer finds a crash:
1. Minimize the input: `afl-tmin` or libFuzzer's `-minimize_crash`
2. Check the sanitizer output for the root cause
3. Determine if it's exploitable (heap overflow? use-after-free? stack buffer overflow?)
4. Create a findings entry with the entity location and a document linking the crash input
