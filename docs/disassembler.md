# Disassembler

The disassembler reads a `.bin` file and prints an annotated listing that shows the slot address, hex encoding, binary bit pattern, and decoded mnemonic for every instruction.

## Usage

```sh
build/bin/disassembler <program.bin>
```

## Output format

```
Disassembly of build/ex/vcl/gcd.bin (44 instructions)
------------------------------------------------------------
Addr   Hex     Binary                 Assembly
------------------------------------------------------------
0000  0xB822  10111 000 001 00010  CALL 34
0001  0xE800  11101 000 000 00000  END
0002  0x5000  01010 000 000 00000  STORE R0, 0
0003  0x5101  01010 001 000 00001  STORE R1, 1
0004  0x4801  01001 000 000 00001  LOAD R0, 1
...
```

Column layout:

| Column | Content |
|---|---|
| `Addr` | Instruction slot index (0-based decimal) |
| `Hex` | 16-bit instruction word in hexadecimal |
| `Binary` | 16-bit word in binary with spaces at format boundaries: `OOOOO RRR RRR XXXXX` |
| `Assembly` | Decoded mnemonic with operands |

The binary column makes it easy to verify the instruction encoding against the ISA format tables. For example, `0xB822`:

```
Bits:  1 0 1 1 1   0 0 0   0 0 1   0 0 0 1 0
       └─────────┘ └─────┘ └─────┘ └─────────┘
       opcode=0x17  unused  unused  addr8=0x22=34
       → CALL 34
```

## Error handling

```sh
build/bin/disassembler missing.bin
# Cannot open: missing.bin
```

An empty file is also rejected with a diagnostic:

```sh
build/bin/disassembler empty.bin
# Empty binary file
```

## API

Declared in `src/disassembler.h`:

```c
#include "disassembler.h"

/* Decode one instruction into buf. Returns buf. */
char *disassemble_instr(uint16_t instr, char *buf, size_t bufsz);

/* Print a full annotated listing of prog to stdout. */
void disassemble_listing(const Program *prog);
```

`disassemble_instr` is useful when you want to decode a single word without printing a full listing — for example, inside a debugger or emulator trace.

See `tests/test_disassembler.c` for 30 unit tests covering every opcode family, the binary string formatter, and the unknown-opcode fallback.
