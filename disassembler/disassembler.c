/*
 * disassembler.c — Disassembler for the Virtual CPU (C99)
 *
 * Reads 16-bit binary instructions and produces a human-readable
 * listing with address, hex, binary, and mnemonic columns.
 *
 * Usage: ./disassembler <program.bin>
 */

#include "disassembler.h"
#include <stdio.h>
#include <string.h>

/* =========================================================
 *  Disassemble one instruction into buf (returns buf)
 * ========================================================= */

char *disassemble_instr(uint16_t instr, char *buf, size_t bufsz) {
    int op     = getOpcode(instr);
    int rd     = getRd(instr);
    int rs     = getRs(instr);
    int imm8   = getImm8(instr);
    int addr8  = getAddr8(instr);

    switch (op) {
        /* R-type */
        case OP_ADD:     snprintf(buf, bufsz, "ADD R%d, R%d",  rd, rs); break;
        case OP_SUB:     snprintf(buf, bufsz, "SUB R%d, R%d",  rd, rs); break;
        case OP_AND:     snprintf(buf, bufsz, "AND R%d, R%d",  rd, rs); break;
        case OP_OR:      snprintf(buf, bufsz, "OR R%d, R%d",   rd, rs); break;
        case OP_XOR:     snprintf(buf, bufsz, "XOR R%d, R%d",  rd, rs); break;
        case OP_CMP:     snprintf(buf, bufsz, "CMP R%d, R%d",  rd, rs); break;
        case OP_MOV_REG: snprintf(buf, bufsz, "MOV R%d, R%d",  rd, rs); break;
        /* I-type */
        case OP_MOV_IMM: snprintf(buf, bufsz, "MOV R%d, %d",   rd, imm8); break;
        case OP_LOAD:    snprintf(buf, bufsz, "LOAD R%d, %d",  rd, imm8); break;
        case OP_STORE:   snprintf(buf, bufsz, "STORE R%d, %d", rd, imm8); break;
        /* J-type (8-bit address) */
        case OP_JUMP:            snprintf(buf, bufsz, "JUMP %d",            addr8); break;
        case OP_JUMP_ZERO:       snprintf(buf, bufsz, "JUMP_ZERO %d",       addr8); break;
        case OP_JUMP_NOT_ZERO:   snprintf(buf, bufsz, "JUMP_NOT_ZERO %d",   addr8); break;
        case OP_JUMP_SIGNED:     snprintf(buf, bufsz, "JUMP_SIGNED %d",     addr8); break;
        case OP_JUMP_NOT_SIGNED: snprintf(buf, bufsz, "JUMP_NOT_SIGNED %d", addr8); break;
        case OP_JUMP_CARRY:      snprintf(buf, bufsz, "JUMP_CARRY %d",      addr8); break;
        case OP_JUMP_NOT_CARRY:  snprintf(buf, bufsz, "JUMP_NOT_CARRY %d",  addr8); break;
        case OP_CALL:            snprintf(buf, bufsz, "CALL %d",            addr8); break;
        /* S-type */
        case OP_PUSH: snprintf(buf, bufsz, "PUSH R%d", rd); break;
        case OP_POP:  snprintf(buf, bufsz, "POP R%d",  rd); break;
        case OP_INC:  snprintf(buf, bufsz, "INC R%d",  rd); break;
        case OP_DEC:  snprintf(buf, bufsz, "DEC R%d",  rd); break;
        /* Z-type */
        case OP_NOP: snprintf(buf, bufsz, "NOP"); break;
        case OP_END: snprintf(buf, bufsz, "END"); break;
        case OP_RET: snprintf(buf, bufsz, "RET"); break;
        default:
            snprintf(buf, bufsz, "??? (opcode %d)", op);
    }
    return buf;
}

/* =========================================================
 *  Binary string helper
 * ========================================================= */

static void to_binary_str(uint16_t w, char *out) {
    /* format: "OOOOO RRR RRR XXXXX" with spaces at bit boundaries */
    int pos = 0;
    for (int i = 15; i >= 0; i--) {
        out[pos++] = ((w >> i) & 1) ? '1' : '0';
        if (i == 11 || i == 8 || i == 5) out[pos++] = ' ';
    }
    out[pos] = '\0';
}

/* =========================================================
 *  Full disassembly listing
 * ========================================================= */

void disassemble_listing(const Program *prog) {
    printf("%-6s %-7s %-22s %s\n", "Addr", "Hex", "Binary", "Assembly");
    printf("%s\n", "------------------------------------------------------------");

    char asmstr[64];
    char binstr[24];
    for (int i = 0; i < prog->size; i++) {
        uint16_t w = prog->data[i];
        to_binary_str(w, binstr);
        disassemble_instr(w, asmstr, sizeof(asmstr));
        printf("%04d  0x%04X  %-20s %s\n", i, w, binstr, asmstr);
    }
}

/* =========================================================
 *  Main
 * ========================================================= */

#ifndef VCPU_TESTING
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: disassembler <program.bin>\n");
        return 1;
    }

    Program prog;
    if (!loadBinary(argv[1], &prog)) return 1;
    if (prog.size == 0) {
        fprintf(stderr, "Empty binary file\n");
        prog_free(&prog);
        return 1;
    }

    printf("Disassembly of %s (%d instructions)\n", argv[1], prog.size);
    printf("------------------------------------------------------------\n");
    disassemble_listing(&prog);

    prog_free(&prog);
    return 0;
}
#endif /* VCPU_TESTING */
