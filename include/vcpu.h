/*
 * vcpu.h — Shared ISA definitions for the Virtual CPU toolchain (C99)
 *
 * ISA:  16-bit fixed-width instructions, 5-bit opcode
 * Regs: R0-R7 (8-bit general purpose)
 * Instruction memory: 256 x 16-bit slots (IP = slot index 0-255)
 * Data memory:        256 x 8-bit bytes
 * Stack: SP starts at 0xFF, grows downward, lives in data memory
 * Flags: Zero (ZF), Sign (SF), Carry (CF)
 *
 * Instruction formats:
 *   R-type:  [opcode:5][Rd:3][Rs:3][unused:5]
 *   I-type:  [opcode:5][Rd:3][imm8:8]
 *   J-type:  [opcode:5][unused:3][addr8:8]   <-- addr8, NOT addr11
 *   S-type:  [opcode:5][Rd:3][unused:8]
 *   Z-type:  [opcode:5][unused:11]
 *
 * CALL pushes 1 byte (IP fits in 8 bits), RET pops 1 byte.
 */

#ifndef VCPU_H
#define VCPU_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Suppress -Wunused-function for static helpers that are not used in every
   translation unit that includes this header. */
#ifdef __GNUC__
#define VCPU_UNUSED __attribute__((unused))
#else
#define VCPU_UNUSED
#endif

/* =========================================================
 *  Opcodes (5-bit, bits [15:11])
 * ========================================================= */

/* R-type: register-register ALU */
#define OP_ADD      0x00   /* 00000 */
#define OP_SUB      0x01   /* 00001 */
#define OP_AND      0x02   /* 00010 */
#define OP_OR       0x03   /* 00011 */
#define OP_XOR      0x04   /* 00100 */
#define OP_CMP      0x05   /* 00101 */
#define OP_MOV_REG  0x06   /* 00110 */
/* 0x07 reserved */

/* I-type: register + immediate */
#define OP_MOV_IMM  0x08   /* 01000 */
#define OP_LOAD     0x09   /* 01001 */
#define OP_STORE    0x0A   /* 01010 */
/* 0x0B-0x0F reserved */

/* J-type: jumps and call (8-bit address) */
#define OP_JUMP            0x10   /* 10000 */
#define OP_JUMP_ZERO       0x11   /* 10001 */
#define OP_JUMP_NOT_ZERO   0x12   /* 10010 */
#define OP_JUMP_SIGNED     0x13   /* 10011 */
#define OP_JUMP_NOT_SIGNED 0x14   /* 10100 */
#define OP_JUMP_CARRY      0x15   /* 10101 */
#define OP_JUMP_NOT_CARRY  0x16   /* 10110 */
#define OP_CALL            0x17   /* 10111 */

/* S-type: single-register ops */
#define OP_PUSH 0x18   /* 11000 */
#define OP_POP  0x19   /* 11001 */
#define OP_INC  0x1A   /* 11010 */
#define OP_DEC  0x1B   /* 11011 */

/* Z-type: zero-operand */
#define OP_NOP  0x1C   /* 11100 */
#define OP_END  0x1D   /* 11101 */
#define OP_RET  0x1E   /* 11110 */
/* 0x1F reserved */

/* =========================================================
 *  Instruction encoding (build)
 * ========================================================= */

static VCPU_UNUSED uint16_t buildR(int op, int rd, int rs) {
    return (uint16_t)(((op & 0x1F) << 11) | ((rd & 0x7) << 8) | ((rs & 0x7) << 5));
}

static VCPU_UNUSED uint16_t buildI(int op, int rd, int imm8) {
    return (uint16_t)(((op & 0x1F) << 11) | ((rd & 0x7) << 8) | (imm8 & 0xFF));
}

static VCPU_UNUSED uint16_t buildJ(int op, int addr8) {
    return (uint16_t)(((op & 0x1F) << 11) | (addr8 & 0xFF));
}

static VCPU_UNUSED uint16_t buildS(int op, int rd) {
    return (uint16_t)(((op & 0x1F) << 11) | ((rd & 0x7) << 8));
}

static VCPU_UNUSED uint16_t buildZ(int op) {
    return (uint16_t)((op & 0x1F) << 11);
}

/* =========================================================
 *  Instruction decoding (extract fields)
 * ========================================================= */

static VCPU_UNUSED int getOpcode(uint16_t w) { return (w >> 11) & 0x1F; }
static VCPU_UNUSED int getRd    (uint16_t w) { return (w >>  8) & 0x07; }
static VCPU_UNUSED int getRs    (uint16_t w) { return (w >>  5) & 0x07; }
static VCPU_UNUSED int getImm8  (uint16_t w) { return  w        & 0xFF; }
static VCPU_UNUSED int getAddr8 (uint16_t w) { return  w        & 0xFF; }  /* J-type: lower 8 bits */

/* =========================================================
 *  Format classification
 * ========================================================= */

typedef enum {
    FMT_R, FMT_I, FMT_J, FMT_S, FMT_Z, FMT_UNKNOWN
} InstrFormat;

static VCPU_UNUSED InstrFormat formatOf(int op) {
    switch (op) {
        case OP_ADD: case OP_SUB: case OP_AND: case OP_OR:
        case OP_XOR: case OP_CMP: case OP_MOV_REG:
            return FMT_R;
        case OP_MOV_IMM: case OP_LOAD: case OP_STORE:
            return FMT_I;
        case OP_JUMP: case OP_JUMP_ZERO: case OP_JUMP_NOT_ZERO:
        case OP_JUMP_SIGNED: case OP_JUMP_NOT_SIGNED:
        case OP_JUMP_CARRY: case OP_JUMP_NOT_CARRY: case OP_CALL:
            return FMT_J;
        case OP_PUSH: case OP_POP: case OP_INC: case OP_DEC:
            return FMT_S;
        case OP_NOP: case OP_END: case OP_RET:
            return FMT_Z;
        default:
            return FMT_UNKNOWN;
    }
}

/* =========================================================
 *  Mnemonic lookup
 * ========================================================= */

static VCPU_UNUSED const char *mnemonicOf(int op) {
    switch (op) {
        case OP_ADD:             return "ADD";
        case OP_SUB:             return "SUB";
        case OP_AND:             return "AND";
        case OP_OR:              return "OR";
        case OP_XOR:             return "XOR";
        case OP_CMP:             return "CMP";
        case OP_MOV_REG:         return "MOV";
        case OP_MOV_IMM:         return "MOV";
        case OP_LOAD:            return "LOAD";
        case OP_STORE:           return "STORE";
        case OP_JUMP:            return "JUMP";
        case OP_JUMP_ZERO:       return "JUMP_ZERO";
        case OP_JUMP_NOT_ZERO:   return "JUMP_NOT_ZERO";
        case OP_JUMP_SIGNED:     return "JUMP_SIGNED";
        case OP_JUMP_NOT_SIGNED: return "JUMP_NOT_SIGNED";
        case OP_JUMP_CARRY:      return "JUMP_CARRY";
        case OP_JUMP_NOT_CARRY:  return "JUMP_NOT_CARRY";
        case OP_CALL:            return "CALL";
        case OP_PUSH:            return "PUSH";
        case OP_POP:             return "POP";
        case OP_INC:             return "INC";
        case OP_DEC:             return "DEC";
        case OP_NOP:             return "NOP";
        case OP_END:             return "END";
        case OP_RET:             return "RET";
        default:                 return "???";
    }
}

/* =========================================================
 *  Shared error list  (assembler, compiler)
 * ========================================================= */

#define MAX_ERRORS 64    /* max number of error messages */
#define MAX_LINE   256   /* max length of one message    */

typedef struct {
    char msgs[MAX_ERRORS][MAX_LINE];
    int  count;
} ErrorList;

/* =========================================================
 *  CPU state
 * ========================================================= */

typedef struct {
    uint8_t  reg[8];    /* R0-R7, 8-bit each */
    uint8_t  mem[256];  /* data memory, 256 bytes */
    uint8_t  sp;        /* stack pointer, starts at 0xFF */
    uint16_t ip;        /* instruction pointer (slot index) */
    int      zero;      /* Zero flag */
    int      sign;      /* Sign flag */
    int      carry;     /* Carry flag */
} CPU;

static VCPU_UNUSED void cpu_reset(CPU *cpu) {
    memset(cpu->reg, 0, sizeof(cpu->reg));
    memset(cpu->mem, 0, sizeof(cpu->mem));
    cpu->sp    = 0xFF;
    cpu->ip    = 0;
    cpu->zero  = 0;
    cpu->sign  = 0;
    cpu->carry = 0;
}

/* =========================================================
 *  Program buffer (dynamic array of uint16_t)
 * ========================================================= */

typedef struct {
    uint16_t *data;
    int       size;
    int       cap;
} Program;

static VCPU_UNUSED void prog_init(Program *p) {
    p->data = NULL;
    p->size = 0;
    p->cap  = 0;
}

static VCPU_UNUSED void prog_free(Program *p) {
    free(p->data);
    p->data = NULL;
    p->size = 0;
    p->cap  = 0;
}

static VCPU_UNUSED int prog_push(Program *p, uint16_t w) {
    if (p->size >= p->cap) {
        int newcap = p->cap < 16 ? 16 : p->cap * 2;
        uint16_t *tmp = (uint16_t *)realloc(p->data, (size_t)newcap * sizeof(uint16_t));
        if (!tmp) return 0;
        p->data = tmp;
        p->cap  = newcap;
    }
    p->data[p->size++] = w;
    return 1;
}

/* =========================================================
 *  Binary I/O helpers
 * ========================================================= */

static VCPU_UNUSED int loadBinary(const char *path, Program *out) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "Cannot open: %s\n", path); return 0; }
    prog_init(out);
    uint8_t hi, lo;
    while (fread(&hi, 1, 1, f) == 1 && fread(&lo, 1, 1, f) == 1) {
        uint16_t w = (uint16_t)((hi << 8) | lo);
        if (!prog_push(out, w)) { fclose(f); return 0; }
    }
    fclose(f);
    return 1;
}

static VCPU_UNUSED int saveBinary(const char *path, const Program *p) {
    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "Cannot open: %s\n", path); return 0; }
    for (int i = 0; i < p->size; i++) {
        uint8_t hi = (uint8_t)((p->data[i] >> 8) & 0xFF);
        uint8_t lo = (uint8_t)( p->data[i]       & 0xFF);
        fwrite(&hi, 1, 1, f);
        fwrite(&lo, 1, 1, f);
    }
    fclose(f);
    return 1;
}

#endif /* VCPU_H */
