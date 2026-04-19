# Testing

The project has 323 automated tests across 7 suites. All tests are plain C99 files that compile to standalone executables — no external test framework is required.

## Running the tests

```sh
make test
```

Output:

```
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
```

Failed tests print the source file, line number, and the expected vs. actual values before the summary.

## Test suites

### test_assembler (65 tests)

Unit tests for `src/assembler.c`. Every instruction format (R, I, J, S, Z) is assembled and the resulting 16-bit word is compared against hand-computed expected values. Additional tests cover:

- Label resolution (forward and backward references)
- Duplicate label detection
- Immediate out-of-range errors
- Unknown mnemonic errors
- MOV with register vs. immediate operand disambiguation
- `MAX_LABELS`, `MAX_LINES`, and `MAX_ERRORS` boundary conditions

### test_disassembler (44 tests)

Unit tests for `src/disassembler.c`. Every opcode is encoded and then decoded, and the mnemonic string is checked. Additional tests cover the binary string formatter at bit boundaries, and the fallback string for unknown opcodes.

### test_emulator (44 tests)

Unit tests for `src/emulator.c`. Tests are grouped by instruction family:

- ALU instructions: correct result and flag values for ADD, SUB, AND, OR, XOR, CMP
- Flag edge cases: carry on addition overflow, zero flag, sign flag
- LOAD / STORE round-trip
- PUSH / POP stack discipline
- All seven conditional jump variants (taken and not-taken)
- CALL / RET return-address preservation
- INC / DEC including wraparound at 255→0 and 0→255
- Stack overflow and underflow detection

### test_integration (16 tests)

End-to-end tests that assemble a multi-instruction program and run it through the emulator, checking R0 and specific memory addresses:

- Factorial (iterative)
- Fibonacci sequence (first 13 values in memory)
- Bubble sort (verifies sorted order in memory)
- GCD via Euclidean subtraction
- A nested-call chain
- Programs that exercise every jump condition

### test_lexer (38 tests)

Unit tests for `src/lexer.c`. Coverage targets every line and branch, verified with `gcov -b`. Tests include:

- All six keywords
- Identifier edge cases: underscore prefix, alphanumeric suffix, names that start with a keyword but are not keywords, truncation at MAX_NAME−1
- Decimal and hexadecimal literals (both `0x` and `0X` prefix)
- All two-character operators (`==`, `!=`, `<=`, `>=`)
- All single-character tokens
- Comment stripping mid-line and comment reaching end-of-file without a trailing newline
- Whitespace and line-counter tracking
- Unknown character → error path, including the `el_add` overflow guard
- Token buffer overflow (more than MAX_TOKENS tokens)

### test_parser (42 tests)

Unit tests for `src/parser.c`. Coverage targets every line and branch. Tests include:

- Every grammatical construct: function declarations (0, 1, 2, 3 parameters), global and local `var`, assignment, `if`, `if/else`, `while`, `return`, call as expression and as statement
- All six comparison operators
- All five arithmetic/bitwise operators
- Left-associative chaining of binary operators
- Parenthesised sub-expressions
- Error paths: integer literal > 255, unexpected token in expression, unexpected token in statement, function with more than 3 parameters, block with more than MAX_CHILDREN statements (the `g_child_overflow` path)

### test_codegen (74 tests)

End-to-end tests that compile a VCL source string, assemble the output, and run it through the emulator. The test verifies the final value of R0. Coverage targets every line and branch of `src/codegen.c`. Tests include:

- All arithmetic and bitwise operators
- Parenthesised expressions
- Global and local variable declaration, default zero initialisation, assignment
- All 6 comparison operators as values (materialised 0/1)
- All 6 comparison operators as `if` conditions (gen_jump_if_false path)
- All 6 comparison operators as `while` conditions
- Function calls with 0, 1, 2, and 3 arguments
- Chained calls
- Standalone call statement (return value discarded)
- Call that modifies a global (side-effect call)
- Larger programs: GCD, iterative Fibonacci, clamp, power, min3
- Error detection: undefined variable in expression, undefined variable in assignment, call with more than 3 arguments, lexer error propagation, parser error propagation, output buffer overflow

## Test framework

All tests use the lightweight framework in `tests/test_framework.h`:

```c
TEST(name) { ... }          // define a test case

ASSERT_EQ(a, b)             // fail if a != b, print both values
ASSERT_TRUE(cond)           // fail if !cond

RUN(name)                   // execute a test case
PRINT_RESULTS()             // print "N test(s) run, M failed" and return
```

A failed assertion prints the source file, line number, and the expected vs. actual values, then continues — the test suite always runs to completion.

## Coverage

The compiler front-end (lexer, parser, codegen) is verified at **100% line coverage and 100% branch coverage** using `gcov -b`. To reproduce:

```sh
# Build with coverage instrumentation
gcc -std=c99 -Wall -Wextra -Wpedantic -DVCPU_TESTING \
    --coverage -fprofile-arcs -ftest-coverage \
    -I include -I src -I tests \
    -o /tmp/test_codegen tests/test_codegen.c

/tmp/test_codegen

# Generate coverage report
gcov -b /tmp/test_codegen.gcda
grep -E "Lines|Branches" codegen.c.gcov
# Lines executed:100.00%
# Branches executed:100.00%
```

## VCPU_TESTING macro

Every tool source file wraps its `main()` function in `#ifndef VCPU_TESTING`. Test binaries are compiled with `-DVCPU_TESTING`, which suppresses the CLI `main()` and lets the test suite define its own `main()`. This means each test binary directly includes the tool's `.c` file and exercises the internal functions without any process boundary.
