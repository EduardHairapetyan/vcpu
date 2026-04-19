# vCPU Toolchain

A complete virtual CPU toolchain written in C99, built as a bachelor's thesis project at Yerevan State University (Faculty of Informatics and Applied Mathematics). The toolchain implements a custom 16-bit ISA from scratch and provides every layer needed to go from high-level source code to a running binary.

```
VCL source  →  compiler  →  assembly  →  assembler  →  binary  →  emulator
                                                            ↑
                                                      disassembler
```

## What is included

| Component | Description |
|---|---|
| **Assembler** | Two-pass assembler. Converts `.asm` source to `.bin` binary. |
| **Disassembler** | Decodes a `.bin` file and prints an annotated listing. |
| **Emulator** | Executes `.bin` programs; includes a step-by-step debug mode. |
| **Compiler** | Compiles VCL (Virtual CPU Language) source to `.asm`. |

All four tools share a single ISA definition (`include/vcpu.h`) and a single error-reporting type (`ErrorList`). They follow an identical source layout and Makefile style.

## Quick start

```sh
# Build all tools
make

# Run all 323 tests
make test

# Compile and run every example, print final R0
make examples

# Full pipeline manually (sum 1..10 = 55)
build/bin/compiler   examples/vcl/counter.vcl   /tmp/out.asm
build/bin/assembler  /tmp/out.asm               /tmp/out.bin
build/bin/emulator   /tmp/out.bin
```

**Requirements:** GCC (or any C99 compiler), GNU Make. No external libraries.

## Repository layout

```
vcpu_unified/
├── Makefile                   single unified build file
├── README.md                  this file
├── include/
│   └── vcpu.h                 shared ISA — opcodes, instruction formats,
│                              CPU state, Program buffer, ErrorList, I/O helpers
├── assembler/
│   ├── assembler.h / .c       two-pass assembler
│   └── README.md
├── disassembler/
│   ├── disassembler.h / .c    listing generator
│   └── README.md
├── emulator/
│   ├── emulator.h / .c        instruction-level interpreter
│   └── README.md
├── compiler/
│   ├── compiler.h             VCL tokens, AST node types, compiler API
│   ├── compiler.c             CLI driver (reads .vcl, writes .asm)
│   ├── lexer.c                tokeniser
│   ├── parser.c               recursive-descent parser + AST pool
│   ├── codegen.c              AST walker → vCPU assembly text
│   └── README.md
├── tests/
│   ├── test_framework.h       lightweight ASSERT/RUN/PRINT_RESULTS macros
│   ├── test_assembler.c       54 unit tests
│   ├── test_disassembler.c    30 unit tests
│   ├── test_emulator.c        30 unit tests
│   ├── test_integration.c     10 end-to-end tests
│   ├── test_lexer.c           38 unit tests
│   ├── test_parser.c          42 unit tests
│   └── test_codegen.c         74 end-to-end compiler tests
└── examples/
    ├── asm/                   hand-written assembly programs
    └── vcl/                   VCL high-level programs
```

## Build targets

| Target | Description |
|---|---|
| `make` | Build all four tool binaries into `build/bin/` |
| `make test` | Build and run all 7 test suites, print a coloured summary |
| `make examples` | Compile/assemble every example, print final R0 value |
| `make clean` | Delete `build/` |

## Architecture overview

The ISA is 16-bit fixed-width with a 5-bit opcode, eight 8-bit registers (R0–R7), 256 instruction slots, and 256 bytes of data memory. Full details are in [`docs/ISA.md`](docs/ISA.md).

VCL is a tiny imperative language that compiles to the ISA. It supports `int` (8-bit unsigned), functions, `if/else`, `while`, and the six comparison operators. Full details and the grammar are in [`docs/VCL.md`](docs/VCL.md).

Each tool is documented in its own file:

- [`docs/assembler.md`](docs/assembler.md)
- [`docs/disassembler.md`](docs/disassembler.md)
- [`docs/emulator.md`](docs/emulator.md)
- [`docs/compiler.md`](docs/compiler.md)

## Test results

  test_assembler          65 tests   PASS
  test_disassembler       44 tests   PASS
  test_emulator           44 tests   PASS
  test_integration        16 tests   PASS
  test_lexer              38 tests   PASS
  test_parser             42 tests   PASS
  test_codegen            74 tests   PASS

════════════════════════════════════════════════
  323 tests run — all passed
════════════════════════════════════════════════

Compiler tests (test_lexer, test_parser, test_codegen) are verified at 100% line and 100% branch coverage via `gcov -b`.
