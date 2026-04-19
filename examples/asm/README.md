# Assembly Examples

Hand-written vCPU assembly programs. Each demonstrates a distinct set of ISA features.

| File | Description | R0 on exit |
|---|---|---|
| `hello.asm` | Stores "HELLO" as ASCII in mem[0..4] | 79 ('O') |
| `factorial.asm` | 5! = 120, subroutine with CALL/RET | 1 |
| `fibonacci.asm` | First 13 Fibonacci numbers in mem[0..12] | 55 |
| `bubble_sort.asm` | Sorts {50,20,40,10,30} → {10,20,30,40,50} in mem[0..4] | 40 |
| `r7_test.asm` | Verifies R7 works correctly in all ALU operations | 150 |

## Build and run

```sh
# From the project root — build all tools first
make

# Assemble and run one file
build/bin/assembler examples/asm/factorial.asm /tmp/factorial.bin
build/bin/emulator  /tmp/factorial.bin

# Run all assembly examples at once
make examples
```

## ISA limitations visible in these examples

Two examples (`fibonacci.asm`, `bubble_sort.asm`) manually unroll inner loops because the ISA has no register-indirect addressing. There is no way to compute a runtime memory address like `mem[R0]` — `LOAD` and `STORE` take a fixed 8-bit address encoded into the instruction. This is the primary motivation for VCL: the compiler manages static address allocation automatically.

See [`docs/ISA.md`](../../docs/ISA.md) for the full instruction set reference.
