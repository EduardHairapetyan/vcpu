# Examples

The project ships with two categories of examples. Assembly examples demonstrate ISA features and idioms directly. VCL examples show the high-level language and the full compiler pipeline.

Run all examples with one command:

```sh
make examples
```

Output:

```
════════════════════════════════════════════════
  Assembly examples
════════════════════════════════════════════════
  bubble_sort        R0 = 40 (0x28)
  factorial          R0 = 1 (0x01)
  fibonacci          R0 = 55 (0x37)
  hello              R0 = 79 (0x4F)
  r7_test            R0 = 150 (0x96)

════════════════════════════════════════════════
  VCL examples
════════════════════════════════════════════════
  add                R0 = 35 (0x23)
  clamp              R0 = 160 (0xA0)
  counter            R0 = 5 (0x05)
  factorial          R0 = 120 (0x78)
  gcd                R0 = 12 (0x0C)
  max                R0 = 42 (0x2A)
  min3               R0 = 17 (0x11)
  popcount           R0 = 5 (0x05)
  power              R0 = 128 (0x80)
```

---

## Assembly examples (`examples/asm/`)

### hello.asm

Stores the ASCII codes for `HELLO` in memory locations 0–4. The vCPU has no I/O instructions, so this is the idiomatic "hello world": inspect `mem[0..4]` after halting.

```
R0 = 79 ('O')   mem[0]=72 mem[1]=69 mem[2]=76 mem[3]=76 mem[4]=79
```

**Demonstrates:** `MOV`, `STORE`, `END`.

---

### factorial.asm

Computes 5! = 120 using a hand-written recursive-style subroutine with `CALL`/`RET`. Multiplication is implemented as repeated addition inside the subroutine.

```
R0 = 1   mem[0] = 120 (0x78)
```

**Demonstrates:** `CALL`, `RET`, `PUSH`, `POP`, `INC`, `DEC`, `CMP`, `JUMP_ZERO`, `JUMP_SIGNED`, conditional and unconditional `JUMP`.

---

### fibonacci.asm

Computes the first 13 Fibonacci numbers and stores them in `mem[0..12]`. Because the ISA has no indexed addressing, the inner loop is fully unrolled — a realistic constraint of the architecture.

```
mem[0..12] = 0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144
R0 = 55   R1 = 89   R2 = 144
```

**Demonstrates:** Register-to-register `MOV`, `ADD`, the absence of indexed addressing as a motivating constraint for the VCL compiler.

---

### bubble_sort.asm

Sorts five values `{50, 20, 40, 10, 30}` stored in `mem[0..4]` using bubble sort. The inner loop is manually unrolled (again, no indexed addressing). Uses a "swapped" flag in R6 to exit early when the array is already sorted.

```
mem[0..4] = 10, 20, 30, 40, 50   (R0 = 40, final last-compared value)
```

**Demonstrates:** Nested loop logic, swap idiom (`LOAD`/`STORE` with two registers), `JUMP_SIGNED`, `JUMP_ZERO`, `DEC`, `JUMP_NOT_ZERO`.

---

### r7_test.asm

Verifies that R7 functions correctly as both source and destination in all five ALU operations. This test was written to validate a fix for a bug in an earlier ISA revision where R7 was incorrectly treated as a sentinel value in the instruction encoder.

```
mem[0] = 150   (ADD R0, R7:  100 + 50)
mem[1] =  50   (SUB R1, R7:  100 - 50)
mem[2] =  32   (AND R2, R7:  0xFF & 0x20)
mem[3] = 255   (OR  R3, R7:  0xCF | 0x30)
mem[4] = 255   (XOR R4, R7:  0xAA ^ 0x55)
```

**Demonstrates:** All R-type ALU operations with R7 as the source operand.

---

## VCL examples (`examples/vcl/`)

### add.vcl

Introduces functions and arithmetic. Calls `add(10, 20)` to get 30, then `add(30, 5)` to get 35. Shows chained function calls and local variable initialisation from a call result.

```
R0 = 35
```

---

### counter.vcl

Introduces global variables and `while` loops. A global `result` is incremented in a loop that runs while `i < 5`.

```
R0 = 5   mem[0] = 5
```

---

### max.vcl

Demonstrates `if/else` and the `>` comparison operator. Returns the larger of two arguments.

```
R0 = 42   (max(42, 17))
```

---

### gcd.vcl

Euclidean GCD via repeated subtraction. Uses `while`, `if/else`, `!=`, and `>`. A good example of a non-trivial algorithm that fits naturally in VCL.

```
R0 = 12   (gcd(48, 36))
```

---

### factorial.vcl

Iterative factorial using a `multiply` helper that implements multiplication via repeated addition (the ISA has no MUL instruction). Shows multi-function programs and the `<=` operator in a loop condition.

```
R0 = 120  (factorial(5) = 5!)
```

---

### clamp.vcl

Restricts a value to `[lo, hi]`. Calls `clamp` three times in `main` and sums the results: `100 + 10 + 50 = 160`. Exercises all three branches (below, in-range, above) and uses both `<` and `>`.

```
R0 = 160
```

---

### power.vcl

Computes `base^exp` using a `multiply` helper and a `power` outer function. Shows two levels of nested function calls and clean separation of concerns.

```
R0 = 128  (power(2, 7) = 2^7)
```

---

### min3.vcl

Finds the minimum of three values by chaining calls to a two-argument `min2` helper: `min2(min2(a, b), c)`. Demonstrates that function call results can be used directly as arguments.

```
R0 = 17   (min3(42, 17, 99))
```

---

### popcount.vcl

Counts the number of set bits in a byte using a loop counter, bitwise `&`, and doubling a mask each iteration to shift it left. Demonstrates bitwise operators in VCL.

```
R0 = 5    (popcount(181) = popcount(0b10110101) = 5 set bits)
```

---

## Running a single example manually

```sh
# ASM example
build/bin/assembler examples/asm/fibonacci.asm /tmp/fib.bin
build/bin/emulator  /tmp/fib.bin

# VCL example — full pipeline
build/bin/compiler   examples/vcl/gcd.vcl       /tmp/gcd.asm
build/bin/assembler  /tmp/gcd.asm               /tmp/gcd.bin
build/bin/emulator   /tmp/gcd.bin

# Inspect the generated assembly
cat /tmp/gcd.asm

# Disassemble the binary
build/bin/disassembler /tmp/gcd.bin

# Step-by-step debug trace
build/bin/emulator /tmp/gcd.bin --debug
```
