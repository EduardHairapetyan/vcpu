# disassembler/

Reads a `.bin` file and prints a human-readable annotated listing.

## Files

| File | Description |
|---|---|
| `disassembler.h` | Public API — exposes `disassemble_instr()` and `disassemble_listing()` |
| `disassembler.c` | Implementation — per-instruction decoder and full-program listing printer |

## Public API

```c
#include "disassembler.h"

/* Disassemble one instruction into buf (size bufsz). Returns buf. */
char *disassemble_instr(uint16_t instr, char *buf, size_t bufsz);

/* Print a full annotated listing of prog to stdout. */
void disassemble_listing(const Program *prog);
```

## CLI usage

```sh
build/bin/disassembler <program.bin>
```

Output columns: address, hex word, 16-bit binary, mnemonic with operands.

See [`../docs/disassembler.md`](../docs/disassembler.md) for sample output and format details.
