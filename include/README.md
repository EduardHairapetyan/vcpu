# include/

## vcpu.h

The single shared header for the entire toolchain. Every source file includes it directly or transitively.

### What it defines

**ISA constants** — one `#define` per opcode with its 5-bit value and a comment showing the binary representation:

```c
#define OP_ADD  0x00   /* 00000 */
#define OP_CALL 0x17   /* 10111 */
// ...
```

**Instruction encoding helpers** — one function per format:

```c
uint16_t buildR(int op, int rd, int rs);
uint16_t buildI(int op, int rd, int imm8);
uint16_t buildJ(int op, int addr8);
uint16_t buildS(int op, int rd);
uint16_t buildZ(int op);
```

**Instruction decoding helpers:**

```c
int getOpcode(uint16_t w);   // bits [15:11]
int getRd    (uint16_t w);   // bits [10:8]
int getRs    (uint16_t w);   // bits [7:5]
int getImm8  (uint16_t w);   // bits [7:0]
int getAddr8 (uint16_t w);   // bits [7:0]
```

**Mnemonic lookup:**

```c
const char *mnemonicOf(int opcode);   // "ADD", "CALL", etc.
```

**Format classification:**

```c
typedef enum { FMT_R, FMT_I, FMT_J, FMT_S, FMT_Z, FMT_UNKNOWN } InstrFormat;
InstrFormat formatOf(int opcode);
```

**CPU state:**

```c
typedef struct {
    uint8_t  reg[8];    // R0-R7
    uint8_t  mem[256];  // data memory
    uint8_t  sp;        // stack pointer (starts at 0xFF)
    uint16_t ip;        // instruction pointer
    int      zero, sign, carry;
} CPU;

void cpu_reset(CPU *cpu);
```

**Program buffer:**

```c
typedef struct { uint16_t *data; int size; int cap; } Program;
void prog_init (Program *p);
void prog_free (Program *p);
int  prog_push (Program *p, uint16_t w);
```

**Binary I/O:**

```c
int loadBinary(const char *path, Program *out);
int saveBinary(const char *path, const Program *p);
```

**Shared error type:**

```c
#define MAX_ERRORS 64
#define MAX_LINE   256
typedef struct { char msgs[MAX_ERRORS][MAX_LINE]; int count; } ErrorList;
```

All functions are `static VCPU_UNUSED` to suppress "unused function" warnings in translation units that include the header but do not use every function.

See [`../docs/ISA.md`](../docs/ISA.md) for the full ISA reference.
