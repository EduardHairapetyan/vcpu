/*
 * emulator.c — Emulator for the Virtual CPU (C99)
 *
 * Executes 16-bit binary programs. Key correctness points:
 *   - J-type uses addr8 (8-bit address, lower byte of instruction)
 *   - CALL pushes 1 byte (IP fits in 8 bits for 256-slot instr memory)
 *   - RET pops 1 byte
 *
 * Usage: ./emulator <program.bin> [--debug]
 */

#include "emulator.h"
#include <stdio.h>
#include <string.h>

/* =========================================================
 *  Display helpers
 * ========================================================= */

static void print_state(const CPU *cpu, int debug) {
    if (!debug) return;
    printf("\n+--------------------------------------+\n");
    printf("|  IP = %3d   SP = %3d   Z=%d S=%d C=%d       |\n",
           (int)cpu->ip, (int)cpu->sp,
           cpu->zero, cpu->sign, cpu->carry);
    printf("+--------------------------------------+\n");
    for (int i = 0; i < 8; i += 2) {
        printf("|  R%d = %3d (0x%02X)   R%d = %3d (0x%02X)  |\n",
               i,   cpu->reg[i],   cpu->reg[i],
               i+1, cpu->reg[i+1], cpu->reg[i+1]);
    }
    printf("+--------------------------------------+\n");
}

/* =========================================================
 *  Flag update
 * ========================================================= */

static void update_flags(CPU *cpu, uint16_t result) {
    cpu->zero  = ((result & 0xFF) == 0);
    cpu->sign  = ((result & 0x80) != 0);
    cpu->carry = (result > 0xFF);
}

/* =========================================================
 *  Stack helpers
 * ========================================================= */

static int stack_push(CPU *cpu, uint8_t value) {
    if (cpu->sp == 0x00) {
        fprintf(stderr, "Stack overflow! SP would go below 0x00\n");
        return 0;
    }
    cpu->mem[cpu->sp] = value;
    cpu->sp--;
    return 1;
}

static int stack_pop(CPU *cpu, uint8_t *out) {
    if (cpu->sp == 0xFF) {
        fprintf(stderr, "Stack underflow! Pop from empty stack\n");
        return 0;
    }
    cpu->sp++;
    *out = cpu->mem[cpu->sp];
    return 1;
}

/* =========================================================
 *  Execute one instruction — returns 1 to continue, 0 to halt
 * ========================================================= */

int execute_one(CPU *cpu, const Program *prog, int debug) {
    if (cpu->ip >= (uint16_t)prog->size) {
        fprintf(stderr, "IP out of bounds: %d\n", (int)cpu->ip);
        return 0;
    }

    uint16_t instr  = prog->data[cpu->ip];
    int      opcode = getOpcode(instr);
    int      rd     = getRd(instr);
    int      rs     = getRs(instr);
    int      imm8   = getImm8(instr);
    int      addr8  = getAddr8(instr);

    if (debug) {
        printf("\n[IP=%3d] 0x%04X  %-16s", (int)cpu->ip, instr, mnemonicOf(opcode));
    }

    cpu->ip++;

    switch (opcode) {

    /* R-type ALU */
    case OP_ADD: case OP_SUB: case OP_AND:
    case OP_OR:  case OP_XOR: case OP_CMP: {
        uint16_t result = 0;
        switch (opcode) {
            case OP_ADD: result = (uint16_t)cpu->reg[rd] + cpu->reg[rs]; break;
            case OP_SUB: result = (uint16_t)cpu->reg[rd] - cpu->reg[rs]; break;
            case OP_AND: result = cpu->reg[rd] & cpu->reg[rs];           break;
            case OP_OR:  result = cpu->reg[rd] | cpu->reg[rs];           break;
            case OP_XOR: result = cpu->reg[rd] ^ cpu->reg[rs];           break;
            case OP_CMP: result = (uint16_t)cpu->reg[rd] - cpu->reg[rs]; break;
        }
        if (opcode != OP_CMP) cpu->reg[rd] = (uint8_t)(result & 0xFF);
        update_flags(cpu, result);
        break;
    }

    /* MOV reg,reg */
    case OP_MOV_REG:
        cpu->reg[rd] = cpu->reg[rs];
        break;

    /* I-type */
    case OP_MOV_IMM: cpu->reg[rd]    = (uint8_t)imm8;         break;
    case OP_LOAD:    cpu->reg[rd]    = cpu->mem[imm8];         break;
    case OP_STORE:   cpu->mem[imm8]  = cpu->reg[rd];           break;

    /* J-type: conditional jumps */
    case OP_JUMP: case OP_JUMP_ZERO: case OP_JUMP_NOT_ZERO:
    case OP_JUMP_SIGNED: case OP_JUMP_NOT_SIGNED:
    case OP_JUMP_CARRY:  case OP_JUMP_NOT_CARRY: {
        int jump = 0;
        switch (opcode) {
            case OP_JUMP:            jump = 1;             break;
            case OP_JUMP_ZERO:       jump = cpu->zero;     break;
            case OP_JUMP_NOT_ZERO:   jump = !cpu->zero;    break;
            case OP_JUMP_SIGNED:     jump = cpu->sign;     break;
            case OP_JUMP_NOT_SIGNED: jump = !cpu->sign;    break;
            case OP_JUMP_CARRY:      jump = cpu->carry;    break;
            case OP_JUMP_NOT_CARRY:  jump = !cpu->carry;   break;
        }
        if (jump) cpu->ip = (uint16_t)addr8;
        break;
    }

    /* CALL: push 1-byte return address, jump to addr8 */
    case OP_CALL:
        if (!stack_push(cpu, (uint8_t)(cpu->ip & 0xFF))) return 0;
        cpu->ip = (uint16_t)addr8;
        break;

    /* S-type */
    case OP_PUSH:
        if (!stack_push(cpu, cpu->reg[rd])) return 0;
        break;

    case OP_POP: {
        uint8_t val;
        if (!stack_pop(cpu, &val)) return 0;
        cpu->reg[rd] = val;
        break;
    }

    case OP_INC: {
        uint16_t r = (uint16_t)cpu->reg[rd] + 1;
        cpu->reg[rd] = (uint8_t)(r & 0xFF);
        update_flags(cpu, r);
        break;
    }

    case OP_DEC: {
        uint16_t r = (uint16_t)cpu->reg[rd] - 1;
        cpu->reg[rd] = (uint8_t)(r & 0xFF);
        update_flags(cpu, r);
        break;
    }

    /* Z-type */
    case OP_NOP:
        break;

    case OP_END:
        if (debug) printf("-> HALT\n");
        return 0;

    /* RET: pop 1-byte return address */
    case OP_RET: {
        uint8_t lo;
        if (!stack_pop(cpu, &lo)) return 0;
        cpu->ip = (uint16_t)lo;
        break;
    }

    default:
        fprintf(stderr, "Unknown opcode: %d at IP=%d\n", opcode, (int)cpu->ip - 1);
        return 0;
    }

    print_state(cpu, debug);
    return 1;
}

/* =========================================================
 *  Run entire program (public API for tests)
 * ========================================================= */

int run_program(CPU *cpu, const Program *prog, int max_steps, int debug) {
    int steps = 0;
    while (steps < max_steps) {
        if (!execute_one(cpu, prog, debug)) break;
        steps++;
    }
    return steps;
}

/* =========================================================
 *  Main
 * ========================================================= */

#ifndef VCPU_TESTING

static void print_memory(const CPU *cpu) {
    printf("\n-- Data memory (non-zero, below stack) --\n");
    int any = 0;
    for (int i = 0; i <= (int)cpu->sp; i++) {
        if (cpu->mem[i] != 0) {
            printf("  mem[%3d] = %3d (0x%02X)\n", i, cpu->mem[i], cpu->mem[i]);
            any = 1;
        }
    }
    if (!any) printf("  (all zero)\n");

    if (cpu->sp < 0xFF) {
        printf("\n-- Stack (SP=0x%02X, grows down from 0xFF) --\n", (int)cpu->sp);
        for (int i = 0xFF; i > (int)cpu->sp; i--)
            printf("  mem[0x%02X] = %3d\n", i, cpu->mem[i]);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: ./emulator <program.bin> [--debug]\n");
        return 1;
    }

    int debug = 0;
    for (int i = 2; i < argc; i++)
        if (strcmp(argv[i], "--debug") == 0) debug = 1;

    Program prog;
    if (!loadBinary(argv[1], &prog)) return 1;
    if (prog.size == 0) {
        fprintf(stderr, "Empty binary file\n");
        prog_free(&prog);
        return 1;
    }

    printf("Loaded %d instruction(s) from %s\n", prog.size, argv[1]);
    if (debug) printf("Debug mode ON\n");

    CPU cpu;
    cpu_reset(&cpu);

    int steps = run_program(&cpu, &prog, 100000, debug);

    if (steps >= 100000)
        fprintf(stderr, "\nWarning: step limit reached - possible infinite loop\n");

    printf("\n======================================\n");
    printf("  Finished in %d step(s)\n", steps);
    printf("======================================\n");
    print_state(&cpu, 1);
    print_memory(&cpu);

    prog_free(&prog);
    return 0;
}

#endif /* VCPU_TESTING */
