# ISA Reference

The vCPU uses a 16-bit fixed-width instruction encoding with a 5-bit opcode field. All definitions live in `include/vcpu.h`.

## Machine parameters

| Parameter | Value |
|---|---|
| Instruction width | 16 bits |
| Instruction memory | 256 slots (IP is an 8-bit slot index, 0–255) |
| Data memory | 256 bytes |
| General-purpose registers | 8 × 8-bit (R0–R7) |
| Stack pointer | SP — starts at 0xFF, grows downward into data memory |
| Flags | Zero (Z), Sign (S), Carry (C) |

## Instruction formats

Five formats cover all 25 instructions (7 opcodes are reserved for future use):

```
 15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0
┌───────────────────┬───────────┬───────────┬───────────────────┐
│   opcode [4:0]    │  Rd [2:0] │  Rs [2:0] │    unused [4:0]   │  R-type
└───────────────────┴───────────┴───────────┴───────────────────┘

┌───────────────────┬───────────┬───────────────────────────────┐
│   opcode [4:0]    │  Rd [2:0] │          imm8 [7:0]           │  I-type
└───────────────────┴───────────┴───────────────────────────────┘

┌───────────────────┬───────────┬───────────────────────────────┐
│   opcode [4:0]    │  unused   │          addr8 [7:0]          │  J-type
└───────────────────┴───────────┴───────────────────────────────┘

┌───────────────────┬───────────┬───────────────────────────────┐
│   opcode [4:0]    │  Rd [2:0] │          unused [7:0]         │  S-type
└───────────────────┴───────────┴───────────────────────────────┘

┌───────────────────┬───────────────────────────────────────────┐
│   opcode [4:0]    │                 unused [10:0]             │  Z-type
└───────────────────┴───────────────────────────────────────────┘
```

## Instruction set

### R-type — register ↔ register ALU (opcode 0x00–0x06)

| Mnemonic | Opcode | Operation | Flags |
|---|---|---|---|
| `ADD Rd, Rs` | 0x00 | Rd ← Rd + Rs | Z, S, C |
| `SUB Rd, Rs` | 0x01 | Rd ← Rd − Rs | Z, S, C |
| `AND Rd, Rs` | 0x02 | Rd ← Rd & Rs | Z, S |
| `OR  Rd, Rs` | 0x03 | Rd ← Rd \| Rs | Z, S |
| `XOR Rd, Rs` | 0x04 | Rd ← Rd ^ Rs | Z, S |
| `CMP Rd, Rs` | 0x05 | sets flags for Rd − Rs, **does not write** Rd | Z, S, C |
| `MOV Rd, Rs` | 0x06 | Rd ← Rs | — |

### I-type — register + 8-bit immediate (opcode 0x08–0x0A)

| Mnemonic | Opcode | Operation | Flags |
|---|---|---|---|
| `MOV Rd, imm8`   | 0x08 | Rd ← imm8 | — |
| `LOAD Rd, addr8` | 0x09 | Rd ← mem[addr8] | — |
| `STORE Rd, addr8`| 0x0A | mem[addr8] ← Rd | — |

imm8 and addr8 are unsigned 8-bit values (0–255).

### J-type — jumps and call (opcode 0x10–0x17)

All jump targets are absolute 8-bit instruction slot addresses (0–255).

| Mnemonic | Opcode | Condition |
|---|---|---|
| `JUMP addr8`           | 0x10 | unconditional |
| `JUMP_ZERO addr8`      | 0x11 | Z = 1 |
| `JUMP_NOT_ZERO addr8`  | 0x12 | Z = 0 |
| `JUMP_SIGNED addr8`    | 0x13 | S = 1 |
| `JUMP_NOT_SIGNED addr8`| 0x14 | S = 0 |
| `JUMP_CARRY addr8`     | 0x15 | C = 1 |
| `JUMP_NOT_CARRY addr8` | 0x16 | C = 0 |
| `CALL addr8`           | 0x17 | push IP (1 byte), jump |

### S-type — single-register (opcode 0x18–0x1B)

| Mnemonic | Opcode | Operation | Flags |
|---|---|---|---|
| `PUSH Rd` | 0x18 | mem[SP] ← Rd; SP-- | — |
| `POP  Rd` | 0x19 | SP++; Rd ← mem[SP] | — |
| `INC  Rd` | 0x1A | Rd ← Rd + 1 | Z, S, C |
| `DEC  Rd` | 0x1B | Rd ← Rd − 1 | Z, S, C |

### Z-type — no operands (opcode 0x1C–0x1E)

| Mnemonic | Opcode | Operation |
|---|---|---|
| `NOP` | 0x1C | no operation |
| `END` | 0x1D | halt execution |
| `RET` | 0x1E | pop 1-byte return address, jump to it |

Opcodes 0x07, 0x0B–0x0F, and 0x1F are **reserved**.

## Flag semantics

Flags are set by ALU instructions (ADD, SUB, AND, OR, XOR, INC, DEC) and by CMP. MOV, LOAD, STORE, and jump instructions do not affect flags.

| Flag | Set when |
|---|---|
| **Z** (Zero) | `(result & 0xFF) == 0` |
| **S** (Sign) | `(result & 0x80) != 0` (bit 7 is set) |
| **C** (Carry)| `result > 0xFF` (for addition) or borrow occurred (for subtraction) |

### Using CMP for comparisons

`CMP Rd, Rs` computes `Rd − Rs` and sets flags without writing the result. The compiler uses this pattern to implement all six comparison operators:

| Comparison | Assembly pattern |
|---|---|
| `a == b` | `CMP a, b` → `JUMP_ZERO` |
| `a != b` | `CMP a, b` → `JUMP_NOT_ZERO` |
| `a < b`  | `CMP a, b` → `JUMP_CARRY` (borrow sets Carry) |
| `a > b`  | `CMP b, a` → `JUMP_CARRY` (swap operands) |
| `a <= b` | `CMP a, b` → `JUMP_ZERO` or `JUMP_CARRY` |
| `a >= b` | `CMP a, b` → `JUMP_NOT_CARRY` |

## Stack

The stack lives in the upper region of data memory. SP starts at 0xFF and grows downward.

- **PUSH Rd**: writes `mem[SP] = Rd`, then `SP--`
- **POP Rd**: increments `SP++`, then reads `Rd = mem[SP]`
- **CALL addr8**: pushes the low byte of the current IP, then jumps to addr8
- **RET**: pops one byte and jumps to it

Stack overflow (SP would go below 0x00) and underflow (POP from an empty stack) are detected at runtime and halt the emulator with an error message.

## Binary file format

Programs are stored as a sequence of big-endian 16-bit words. Each instruction occupies exactly 2 bytes: the high byte first, then the low byte. There is no header.

```
Byte 0: high byte of instruction 0
Byte 1: low  byte of instruction 0
Byte 2: high byte of instruction 1
...
```
