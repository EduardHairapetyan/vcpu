# Tests

323 automated tests across 7 suites. Every suite is a single C99 file that compiles to a standalone executable.

```sh
make test        # build and run all suites
```

## Suites

| File | Tests | What it covers |
|---|---|---|
| `test_assembler.c` | 54 | Every instruction encoding, label resolution, error detection |
| `test_disassembler.c` | 30 | Every opcode decoded, binary string formatter, unknown-opcode fallback |
| `test_emulator.c` | 30 | ALU operations, flag semantics, stack, all jump conditions, CALL/RET |
| `test_integration.c` | 10 | Assemble + run full programs, verify R0 and memory contents |
| `test_lexer.c` | 38 | All token types, comments, whitespace, error paths, buffer overflow |
| `test_parser.c` | 42 | Full grammar, all operators, every error branch, capacity guards |
| `test_codegen.c` | 74 | Compile + assemble + run; all operators as conditions and values |

## Test framework (`test_framework.h`)

```c
TEST(name) { ... }              // define a test case

ASSERT_EQ(a, b)                 // fail if a != b
ASSERT_TRUE(cond)               // fail if !cond
ASSERT_FALSE(cond)              // fail if cond
ASSERT_STR_CONTAINS(str, sub)   // fail if sub not in str

RUN(name)                       // execute a test case
PRINT_RESULTS()                 // print summary and return exit code
```

On failure, ASSERT macros print the source file, line number, and expected vs. actual values, then continue — every test runs to completion.

## How tests include tool code

Each test file includes the tool's `.c` file directly:

```c
#define VCPU_TESTING     // suppresses the tool's main()
#include "assembler.c"   // includes the full implementation
```

Each component lives in its own directory (`assembler/`, `disassembler/`, `emulator/`, `compiler/`). The Makefile passes `-I <component>` for each directory so test files can include component sources by bare filename without any path prefix.

This allows tests to call internal functions (e.g. `tokenise`, `encode`, `label_find`) without any process boundary or dynamic linking. The `-DVCPU_TESTING` flag is applied automatically by `make test`.

## Coverage

The compiler front-end is verified at 100% line and 100% branch coverage via `gcov`. See [`docs/testing.md`](../docs/testing.md) for the reproduction steps.
