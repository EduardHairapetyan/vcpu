# Assembler

The assembler converts `.asm` source files to `.bin` binaries. It is a classic two-pass assembler: the first pass tokenises source lines and collects label addresses; the second pass encodes every instruction to its 16-bit binary representation.

## Usage

```sh
build/bin/assembler <input.asm> <output.bin>
```

On success it prints:

```
Assembled 33 instruction(s) -> output.bin
```

On error it prints one or more error messages to stderr and exits with code 1:

```
Line 3: Unknown mnemonic: HALT
Line 7: immediate out of range 0-255
```

## Source file format

Source files are plain text. Each line contains at most one instruction. Lines are case-insensitive for mnemonics and register names.

### Comments

Everything from `;` to end-of-line is a comment.

```asm
    MOV R0, 42      ; load the answer
```

### Labels

A label is an identifier followed by `:`. It may appear alone on a line or before an instruction. Labels are resolved to the instruction slot address of the next instruction.

```asm
loop:
    INC R0
    CMP R0, R1
    JUMP_NOT_ZERO loop
```

Label names are case-sensitive and must be unique within a file.

### Instructions

```asm
    MNEMONIC [arg1[, arg2]]
```

Registers are written `R0`–`R7` (case-insensitive). Immediate values and addresses are decimal by default; the `0x` prefix selects hexadecimal; the `0b` prefix selects binary.

```asm
    MOV  R0, 255        ; decimal
    MOV  R1, 0xFF       ; same value, hex
    MOV  R2, 0b11111111 ; same value, binary
    JUMP loop           ; label reference
    JUMP 5              ; direct slot address
```

## Complete instruction reference

### Register–register (R-type)

```asm
ADD  Rd, Rs     ; Rd = Rd + Rs
SUB  Rd, Rs     ; Rd = Rd - Rs
AND  Rd, Rs     ; Rd = Rd & Rs
OR   Rd, Rs     ; Rd = Rd | Rs
XOR  Rd, Rs     ; Rd = Rd ^ Rs
CMP  Rd, Rs     ; set flags for Rd - Rs (Rd unchanged)
MOV  Rd, Rs     ; Rd = Rs
```

### Register–immediate (I-type)

```asm
MOV   Rd, imm8       ; Rd = imm8        (0–255)
LOAD  Rd, addr8      ; Rd = mem[addr8]  (0–255)
STORE Rd, addr8      ; mem[addr8] = Rd
```

LOAD and STORE also accept label names as the address argument, which lets you name memory locations:

```asm
result:          ; "result" resolves to the next instruction address
    NOP          ; placeholder — occupy a slot so the label has an address

; ... elsewhere:
    LOAD R0, result
```

> **Note:** Because labels resolve to *instruction* addresses, using them as data addresses only works reliably for small programs where instruction and data addresses happen to overlap. For clarity, use numeric addresses for data memory.

### Jumps and call (J-type)

```asm
JUMP            addr8_or_label
JUMP_ZERO       addr8_or_label
JUMP_NOT_ZERO   addr8_or_label
JUMP_SIGNED     addr8_or_label
JUMP_NOT_SIGNED addr8_or_label
JUMP_CARRY      addr8_or_label
JUMP_NOT_CARRY  addr8_or_label
CALL            addr8_or_label
```

### Single-register (S-type)

```asm
PUSH Rd     ; mem[SP] = Rd; SP--
POP  Rd     ; SP++; Rd = mem[SP]
INC  Rd     ; Rd++
DEC  Rd     ; Rd--
```

### Zero-operand (Z-type)

```asm
NOP         ; no operation
END         ; halt
RET         ; pop return address and jump
```

## Limits

| Resource | Limit |
|---|---|
| Instructions per program | 256 (instruction memory size) |
| Labels per file | 256 |
| Source lines per file | 512 |
| Arguments per instruction | 4 |
| Error messages reported | 64 |

## Example

```asm
; sum.asm — compute 1+2+3+4+5 and store result in mem[0]

    MOV R0, 0       ; accumulator
    MOV R1, 1       ; counter
    MOV R2, 6       ; loop limit

loop:
    ADD R0, R1
    INC R1
    CMP R1, R2
    JUMP_NOT_ZERO loop

    STORE R0, 0     ; mem[0] = 15
    END
```

```sh
build/bin/assembler sum.asm sum.bin
# Assembled 9 instruction(s) -> sum.bin
```

## API

For use in other tools and tests, the public API is declared in `src/assembler.h`:

```c
#include "assembler.h"

int assemble(const char *source, Program *out, ErrorList *el);
```

`source` is a null-terminated string containing the full assembly source text. On success the function returns 1 and fills `out`. On error it returns 0 and appends messages to `el`. `el->count == 0` on entry is the caller's responsibility.

See `tests/test_assembler.c` for 54 unit tests covering every instruction encoding, label resolution, duplicate labels, range checks, and error paths.
