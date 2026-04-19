# emulator/

Executes `.bin` programs on the virtual CPU. Includes a step-by-step debug mode.

## Files

| File | Description |
|---|---|
| `emulator.h` | Public API — exposes `execute_one()` and `run_program()` |
| `emulator.c` | Implementation — full instruction interpreter for all ISA opcodes |

## Public API

```c
#include "emulator.h"

/* Execute one instruction. Returns 1 to continue, 0 to halt. */
int execute_one(CPU *cpu, const Program *prog, int debug);

/* Run at most max_steps instructions. Returns actual step count. */
int run_program(CPU *cpu, const Program *prog, int max_steps, int debug);
```

## CLI usage

```sh
build/bin/emulator <program.bin>
build/bin/emulator <program.bin> --debug    # step-by-step trace
```

After execution the final register state (R0–R7, flags, SP) is printed to stdout.

See [`../docs/emulator.md`](../docs/emulator.md) for debug output format and execution semantics.
