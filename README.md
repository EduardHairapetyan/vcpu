# vcpu

A virtual CPU toolchain written in C99 — built as a bachelor's thesis at Yerevan State University.

Implements a custom 16-bit ISA from scratch, with every layer needed to go from source code to a running binary: assembler, disassembler, emulator, and a small compiled language (VCL).

```
VCL source → compiler → assembly → assembler → binary → emulator
                                                    ↑
                                              disassembler
```

A full writeup — from silicon and transistors up through the compiler — is in [`article.pdf`](article.pdf).

---

## Components

| | |
|---|---|
| **Assembler** | Two-pass. Converts `.asm` to `.bin`. |
| **Disassembler** | Decodes `.bin` and prints an annotated listing. |
| **Emulator** | Executes `.bin`; includes step-by-step debug mode. |
| **Compiler** | Compiles VCL source to `.asm`. |

All four tools share a single ISA definition (`include/vcpu.h`).

## Quick start

```sh
make              # build all tools
make test         # run all 323 tests
make examples     # compile and run every example, print final R0

# Full pipeline (sum 1..10 = 55)
build/bin/compiler   examples/vcl/counter.vcl   /tmp/out.asm
build/bin/assembler  /tmp/out.asm               /tmp/out.bin
build/bin/emulator   /tmp/out.bin
```

**Requirements:** GCC (or any C99 compiler), GNU Make. No external libraries.

## Layout

```
├── include/vcpu.h        shared ISA — opcodes, formats, CPU state
├── assembler/
├── disassembler/
├── emulator/
├── compiler/
│   ├── lexer.c
│   ├── parser.c
│   └── codegen.c
├── tests/                323 tests across 7 suites
├── examples/
│   ├── asm/              hand-written assembly programs
│   └── vcl/              VCL source programs
├── docs/                 ISA, VCL grammar, per-tool docs
├── Makefile
└── article.pdf
```

## Tests

```
test_assembler      65 tests   PASS
test_disassembler   44 tests   PASS
test_emulator       44 tests   PASS
test_integration    16 tests   PASS
test_lexer          38 tests   PASS
test_parser         42 tests   PASS
test_codegen        74 tests   PASS

323 tests run — all passed
```

Compiler tests verified at 100% line and branch coverage via `gcov -b`.
