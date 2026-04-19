# VCL Examples

VCL (Virtual CPU Language) programs. Each is compiled to assembly by the compiler and then assembled to binary.

| File | Description | R0 on exit |
|---|---|---|
| `add.vcl` | Function call and arithmetic: `add(add(10,20), 5)` | 35 |
| `counter.vcl` | Global variable incremented in a while loop | 5 |
| `max.vcl` | `if/else` returning the larger of two values | 42 |
| `gcd.vcl` | Euclidean GCD via repeated subtraction | 12 |
| `factorial.vcl` | Iterative factorial using a multiply helper | 120 |
| `clamp.vcl` | Clamp a value to `[lo, hi]`, three calls summed | 160 |
| `power.vcl` | Integer exponentiation using nested function calls | 128 |
| `min3.vcl` | Minimum of three values via chained `min2` calls | 17 |
| `popcount.vcl` | Count set bits in a byte using bitwise AND and a mask | 5 |

## Build and run

```sh
# From the project root — build all tools first
make

# Full pipeline for one file
build/bin/compiler   examples/vcl/gcd.vcl   /tmp/gcd.asm
build/bin/assembler  /tmp/gcd.asm           /tmp/gcd.bin
build/bin/emulator   /tmp/gcd.bin

# Inspect the generated assembly
cat /tmp/gcd.asm

# Run with step-by-step debug trace
build/bin/emulator /tmp/gcd.bin --debug

# Run all VCL examples at once
make examples
```

## Language quick reference

```vcl
// types: int only (8-bit unsigned, 0-255)

var g = 10;               // global variable

func add(a, b) {          // max 3 parameters
    return a + b;         // R0 holds the return value
}

func main() {
    var x = add(3, 4);    // local variable
    if (x > 5) {
        x = x - 1;
    } else {
        x = 0;
    }
    while (x != 0) {
        x = x - 1;
    }
    return x;
}
```

Operators: `+  -  &  |  ^  ==  !=  <  >  <=  >=`

No multiplication, division, shift, pointers, arrays, or recursion — all intentional
constraints that match the ISA. See [`docs/compiler.md`](../../docs/compiler.md) for
the full language reference.
