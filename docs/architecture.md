# Architecture

This document describes the internal design of the toolchain: how the tools are structured, how they share code, and the key design decisions.

## Source layout

```
include/
└── vcpu.h              shared ISA — every source file includes this

src/
├── assembler.h / .c    two-pass assembler
├── disassembler.h / .c listing generator
├── emulator.h / .c     instruction interpreter
├── compiler.h          VCL types and API (includes vcpu.h)
├── lexer.c             tokeniser
├── parser.c            recursive-descent parser + AST node pool
├── codegen.c           AST walker → assembly text
└── compiler.c          CLI driver

tests/
├── test_framework.h    ASSERT/RUN/PRINT_RESULTS macros
└── test_*.c            one test file per source file
```

Every tool follows the same pattern:

```
tool.h     declares the public API — included by tests and by tools
           that depend on this tool
tool.c     #include "tool.h" at the top
           implementation
           #ifndef VCPU_TESTING
           int main(...) { ... }
           #endif
```

The `VCPU_TESTING` guard lets test binaries include a tool's `.c` file directly so they can call internal functions without a process boundary. It suppresses the tool's `main()` so the test binary can define its own.

---

## include/vcpu.h — the shared foundation

`vcpu.h` is the single source of truth for the entire ISA. Every other source file includes it. It defines:

- **Opcode constants** (`OP_ADD`, `OP_SUB`, …) — 5-bit values matching the binary encoding
- **Instruction format enum** (`FMT_R`, `FMT_I`, `FMT_J`, `FMT_S`, `FMT_Z`)
- **Bit-field constructors** (`buildR`, `buildI`, `buildJ`, `buildS`, `buildZ`)
- **Bit-field accessors** (`getOpcode`, `getRd`, `getRs`, `getImm8`, `getAddr8`)
- **Mnemonic lookup** (`mnemonicOf`) — maps opcode to string, used by emulator and disassembler
- **`CPU` struct** — registers, memory, SP, IP, flags
- **`cpu_reset`** — sets the CPU to its startup state
- **`Program` struct** — a dynamic array of `uint16_t` instruction words
- **`prog_init`, `prog_free`, `prog_push`** — Program buffer management
- **`loadBinary`, `saveBinary`** — big-endian 16-bit binary I/O
- **`ErrorList`** — shared error-reporting type (64 messages of up to 256 characters each)

All functions in `vcpu.h` are declared `static` with a `VCPU_UNUSED` attribute. This suppresses "unused function" warnings in translation units that include the header but do not use every function.

---

## Assembler internals

The assembler is a classic two-pass design.

**Pass 1 — tokenise and collect labels:**

Each source line is read and passed to `tokenise()`, which strips comments, extracts an optional label, extracts the mnemonic, and splits comma-separated arguments. Labels are added to a `LabelTable` (an array of `{name, addr}` pairs) with the current instruction address. Lines that produce an instruction increment the address counter; label-only lines and blank lines do not.

**Pass 2 — encode:**

Each tokenised `Line` is passed to `encode()`, which matches the mnemonic to an opcode, validates operand count and types, and calls the appropriate `buildX()` constructor from `vcpu.h`. Jump targets and LOAD/STORE addresses that are label names are resolved via `label_find()`. The resulting 16-bit words are appended to a `Program` via `prog_push()`.

Key data structures (all stack-allocated, no heap):

| Type | Purpose | Capacity |
|---|---|---|
| `LabelTable` | name → instruction address | 256 entries |
| `LineList` | tokenised source lines | 512 entries |
| `Line` | mnemonic + up to 4 arguments | — |

---

## Disassembler internals

The disassembler has two entry points:

`disassemble_instr(uint16_t instr, char *buf, size_t bufsz)` — decodes one instruction word into a human-readable string using `getOpcode()`, `getRd()`, `getRs()`, `getImm8()`, and `getAddr8()` from `vcpu.h`. This is a pure function with no side effects.

`disassemble_listing(const Program *prog)` — iterates over all instruction words, calls `disassemble_instr` for the mnemonic, generates the 16-bit binary string with field separators via `to_binary_str()`, and prints the formatted table.

---

## Emulator internals

The emulator models the vCPU state exactly as the ISA specifies. The core function is `execute_one()`, which:

1. Reads `prog->data[cpu->ip]`
2. Extracts all fields with `getOpcode`/`getRd`/etc.
3. Increments `cpu->ip` (before executing, so CALL can push the correct return address)
4. Dispatches on `opcode` in a `switch` statement
5. Calls `update_flags()` for ALU instructions

Flag update:

```c
cpu->zero  = ((result & 0xFF) == 0);
cpu->sign  = ((result & 0x80) != 0);
cpu->carry = (result > 0xFF);
```

`result` is a `uint16_t` holding the full 9-bit result before truncation to 8 bits. This gives natural carry/borrow detection for addition and subtraction.

The stack:

- `stack_push(cpu, value)` — writes `mem[SP]`, decrements SP. Checks for underflow (SP == 0x00).
- `stack_pop(cpu, &out)` — increments SP, reads `mem[SP]`. Checks for overflow (SP == 0xFF).

`CALL` pushes the low byte of IP (which is always 0–255 since instruction memory has 256 slots), then sets IP to the target address. `RET` pops one byte and jumps to it.

---

## Compiler internals

The compiler is split into four files. The `compiler.c` driver calls them in sequence.

### Lexer (`lexer.c`)

The lexer is a hand-written character scanner. It walks the source string once and produces a flat array of `Token` values. Each token carries its type (a `TokenType` enum value), its text (at most `MAX_NAME-1` characters), and its source line number.

The character `//` starts a comment that consumes to end-of-line or end-of-string. Unknown characters record an error via `el_add()` and continue scanning so the lexer can report multiple errors in one pass.

### Parser (`parser.c`)

The parser is a hand-written recursive descent parser. It maintains a `Parser` struct that holds a pointer to the token array and a position counter. It produces an `ASTNode` tree using a static pool of `MAX_NODES` (1024) nodes — no heap allocation.

Grammar entry points:

| Function | Parses |
|---|---|
| `parse()` | full program — top-level `func` and `var` declarations |
| `parse_func()` | one function declaration |
| `parse_block()` | a `{ stmt* }` block |
| `parse_stmt()` | one statement |
| `parse_expr()` | one expression (comparison level) |
| `parse_addition()` | arithmetic/bitwise level |
| `parse_primary()` | number literal, identifier, call, or `(expr)` |

Errors are non-fatal where possible: the parser records a message in the `ErrorList` and attempts to continue. The caller checks `el->count` after `parse()` returns.

### Code Generator (`codegen.c`)

The code generator is an AST walker that writes assembly text into a caller-supplied buffer via a small `OutBuf` helper that tracks overflow. It maintains two `SymTable` instances (globals and locals) and allocates data-memory addresses monotonically from address 0.

**Two-pass codegen:**

1. Walk the program node's children for `VAR_DECL` nodes and allocate global addresses. This ensures globals have stable addresses before any function body is emitted.
2. Emit the `__start:` preamble (global initialisers, `CALL main`, `END`).
3. Emit each function body.

**Expression strategy:** All expressions leave their result in R0. Binary operations use a PUSH/POP stack discipline: evaluate right into R0, PUSH; evaluate left into R0; POP R1; apply operation. This is safe for arbitrarily nested expressions.

**Comparison strategy:** Comparisons used as `if`/`while` conditions emit `CMP` directly followed by conditional jumps (`gen_jump_if_false`). Comparisons used as values materialise a 0 or 1 into R0.

**Label generation:** Every synthetic label is of the form `_LN` where N is a monotonically increasing integer from `cg->label_cnt`. This avoids conflicts with user-defined labels and with the `__start` entry-point label.

---

## Include dependency graph

```
vcpu.h
 ├── assembler.h   (includes vcpu.h)
 │    └── assembler.c
 ├── disassembler.h (includes vcpu.h)
 │    └── disassembler.c
 ├── emulator.h    (includes vcpu.h)
 │    └── emulator.c
 └── compiler.h    (includes vcpu.h)
      ├── lexer.c
      ├── parser.c
      ├── codegen.c
      └── compiler.c  (#include "lexer.c" "parser.c" "codegen.c")
```

The compiler uses unity-build style: `compiler.c` `#include`s the three implementation files directly. This keeps the build command simple (one `-o compiler compiler.c`) while still allowing each sub-file to be tested independently via `-DVCPU_TESTING`.

---

## Makefile design

A single `Makefile` at the project root handles everything. The key principles:

- One set of `CFLAGS` applied to every compilation: `-std=c99 -Wall -Wextra -Wpedantic`
- One `-I` flag set applied to every compilation: `-I include -I src -I tests`
- Test binaries are built with the additional `-DVCPU_TESTING` flag
- No recursive make — all rules are in one file
- `.PRECIOUS` marks intermediate VCL `.asm` files so they are kept after `make examples`

The `test` target builds all test binaries, runs each one, parses the `N test(s) run, M failed` summary line from its output, and accumulates totals. It exits with code 1 if any suite fails.
