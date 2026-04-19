# assembler/

Two-pass assembler that converts `.asm` source to a `.bin` binary.

## Files

| File | Description |
|---|---|
| `assembler.h` | Public API — exposes `assemble()` |
| `assembler.c` | Implementation — pass 1 collects labels, pass 2 encodes instructions |

## Public API

```c
#include "assembler.h"

int assemble(const char *source, Program *out, ErrorList *el);
```

Returns 1 on success, 0 on error. Errors are written into `el`.

## CLI usage

```sh
build/bin/assembler <input.asm> <output.bin>
```

## How it works

**Pass 1** — scan every line, record label → address mappings in a table.
**Pass 2** — re-scan every line, encode each instruction to its 16-bit word using the label table to resolve forward references.

See [`../docs/assembler.md`](../docs/assembler.md) for the full instruction reference and error list.
