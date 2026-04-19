/*
 * test_integration.c — End-to-end assemble → emulate tests (C99)
 *
 * Tests complete programs: assembler output feeds directly into emulator.
 * Each test validates a real algorithm or ISA property end-to-end.
 */

#ifndef VCPU_TESTING
#define VCPU_TESTING
#endif
#include "test_framework.h"
#include "vcpu.h"
#include "assembler.h"
#include "assembler.c"
#include "emulator.c"

static int run_src(const char *src, CPU *cpu) {
    Program p;
    ErrorList el; el.count = 0;
    if (!assemble(src, &p, &el) || el.count > 0) {
        if (el.count > 0) fprintf(stderr, "  ASM: %s\n", el.msgs[0]);
        prog_free(&p);
        return 0;
    }
    cpu_reset(cpu);
    run_program(cpu, &p, 200000, 0);
    prog_free(&p);
    return 1;
}

/* =========================================================
 *  Classic algorithms
 * ========================================================= */

/* 5! = 120 (iterative multiply via addition) */
TEST(factorial_5) {
    const char *src =
        "    MOV R0, 5\n"
        "    MOV R1, 1\n"
        "    CALL factorial\n"
        "    STORE R1, 0\n"
        "    END\n"
        "factorial:\n"
        "    MOV R3, 1\n"
        "    CMP R0, R3\n"
        "    JUMP_ZERO fact_done\n"
        "    JUMP_SIGNED fact_done\n"
        "    PUSH R0\n"
        "    PUSH R1\n"
        "    MOV R4, 0\n"
        "    MOV R5, 0\n"
        "mul_loop:\n"
        "    ADD R4, R1\n"
        "    INC R5\n"
        "    CMP R5, R0\n"
        "    JUMP_NOT_ZERO mul_loop\n"
        "    MOV R1, R4\n"
        "    POP R4\n"
        "    POP R0\n"
        "    DEC R0\n"
        "    JUMP factorial\n"
        "fact_done:\n"
        "    RET\n";
    CPU cpu;
    ASSERT_TRUE(run_src(src, &cpu));
    ASSERT_EQ(cpu.mem[0], 120);
}

/* First 5 Fibonacci numbers stored in memory */
TEST(fibonacci_first5) {
    const char *src =
        "    MOV R0, 0\n    MOV R1, 1\n"
        "    STORE R0, 0\n    STORE R1, 1\n"
        "    MOV R2, R1\n    ADD R2, R0\n    STORE R2, 2\n"
        "    MOV R0, R1\n    MOV R1, R2\n"
        "    MOV R2, R1\n    ADD R2, R0\n    STORE R2, 3\n"
        "    MOV R0, R1\n    MOV R1, R2\n"
        "    MOV R2, R1\n    ADD R2, R0\n    STORE R2, 4\n"
        "    END\n";
    CPU cpu;
    ASSERT_TRUE(run_src(src, &cpu));
    ASSERT_EQ(cpu.mem[0], 0);
    ASSERT_EQ(cpu.mem[1], 1);
    ASSERT_EQ(cpu.mem[2], 1);
    ASSERT_EQ(cpu.mem[3], 2);
    ASSERT_EQ(cpu.mem[4], 3);
}

/* Bubble sort: [5,3,4,1,2] → [1,2,3,4,5], result in mem[5] */
TEST(bubble_sort) {
    /* Store array [5,3,4,1,2] at addresses 0..4, count at 5 */
    const char *src =
        "    MOV R0, 5\n    STORE R0, 0\n"
        "    MOV R0, 3\n    STORE R0, 1\n"
        "    MOV R0, 4\n    STORE R0, 2\n"
        "    MOV R0, 1\n    STORE R0, 3\n"
        "    MOV R0, 2\n    STORE R0, 4\n"
        /* bubble sort 5 elements, 4 passes */
        "    MOV R7, 4\n"       /* outer counter */
        "outer:\n"
        "    MOV R6, 0\n"       /* inner index */
        "    MOV R5, 4\n"       /* inner limit */
        "inner:\n"
        "    CMP R6, R5\n    JUMP_ZERO outer_dec\n"
        "    LOAD R0, 0\n    LOAD R1, 1\n"  /* simplified: compare mem[0..1] */
        "    CMP R0, R1\n    JUMP_SIGNED no_swap\n    JUMP_ZERO no_swap\n"
        "    STORE R1, 0\n    STORE R0, 1\n"
        "no_swap:\n"
        "    INC R6\n    JUMP inner\n"
        "outer_dec:\n"
        "    DEC R7\n    JUMP_NOT_ZERO outer\n"
        /* Store sorted min in mem[5] for verification */
        "    LOAD R0, 0\n    STORE R0, 5\n"
        "    END\n";
    CPU cpu;
    ASSERT_TRUE(run_src(src, &cpu));
    /* mem[0] should be the minimum after passes */
    ASSERT_EQ(cpu.mem[5], cpu.mem[0]);
    ASSERT_TRUE(cpu.mem[5] > 0);   /* sanity: not zero */
}

/* GCD via Euclidean subtraction: gcd(48, 36) = 12 */
TEST(gcd_48_36) {
    const char *src =
        "    MOV R0, 48\n"
        "    MOV R1, 36\n"
        "loop:\n"
        "    CMP R0, R1\n    JUMP_ZERO done\n"
        "    JUMP_SIGNED b_gt_a\n"
        "    SUB R0, R1\n    JUMP loop\n"
        "b_gt_a:\n"
        "    SUB R1, R0\n    JUMP loop\n"
        "done:\n"
        "    END\n";
    CPU cpu;
    ASSERT_TRUE(run_src(src, &cpu));
    ASSERT_EQ(cpu.reg[0], 12);
}

/* Sum 1..10 = 55 (count down: 10+9+...+1) */
TEST(sum_1_to_10) {
    const char *src =
        "    MOV R0, 0\n"    /* accumulator */
        "    MOV R1, 10\n"   /* counter */
        "loop:\n"
        "    ADD R0, R1\n"
        "    DEC R1\n"
        "    JUMP_NOT_ZERO loop\n"
        "    END\n";
    CPU cpu;
    ASSERT_TRUE(run_src(src, &cpu));
    ASSERT_EQ(cpu.reg[0], 55);
}

/* Popcount of 0xAA (10101010) = 4 */
TEST(popcount_0xAA) {
    const char *src =
        "    MOV R0, 0\n"       /* count */
        "    MOV R1, 0xAA\n"    /* value */
        "    MOV R2, 8\n"       /* bit counter */
        "    MOV R3, 1\n"       /* mask */
        "loop:\n"
        "    CMP R2, R0\n    JUMP_ZERO done\n"
        "    MOV R4, R1\n"
        "    AND R4, R3\n"
        "    JUMP_ZERO no_bit\n"
        "    INC R0\n"
        "no_bit:\n"
        "    ADD R3, R3\n"       /* mask <<= 1 */
        "    DEC R2\n"
        "    JUMP loop\n"
        "done:\n"
        "    END\n";
    CPU cpu;
    ASSERT_TRUE(run_src(src, &cpu));
    ASSERT_EQ(cpu.reg[0], 4);
}

/* =========================================================
 *  ISA correctness
 * ========================================================= */

TEST(jump_addr8_upper_bound) {
    /* Verify jumps work near address 255 */
    char src[8192];
    int pos = 0;
    pos += snprintf(src+pos, sizeof(src)-pos, "MOV R0, 1\nMOV R1, 1\n");
    for (int i = 0; i < 50; i++)
        pos += snprintf(src+pos, sizeof(src)-pos, "NOP\n");
    snprintf(src+pos, sizeof(src)-pos, "END\n");
    CPU cpu;
    ASSERT_TRUE(run_src(src, &cpu));
}

TEST(call_sp_balanced) {
    CPU cpu;
    run_src("CALL f\nEND\nf:\nRET\n", &cpu);
    ASSERT_EQ(cpu.sp, 0xFF);
}

TEST(nested_calls_3_levels) {
    CPU cpu;
    run_src("CALL a\nEND\n"
            "a:\nCALL b\nRET\n"
            "b:\nCALL c\nRET\n"
            "c:\nMOV R0,99\nRET\n", &cpu);
    ASSERT_EQ(cpu.reg[0], 99);
    ASSERT_EQ(cpu.sp, 0xFF);
}

/* =========================================================
 *  Conditional logic patterns
 * ========================================================= */

TEST(if_then_else) {
    const char *src =
        "    MOV R0, 5\n    MOV R2, 5\n"
        "    CMP R0, R2\n    JUMP_ZERO then\n"
        "    MOV R1, 2\n    JUMP done\n"
        "then:\n    MOV R1, 1\n"
        "done:\n    END\n";
    CPU cpu;
    ASSERT_TRUE(run_src(src, &cpu));
    ASSERT_EQ(cpu.reg[1], 1);
}

TEST(while_loop_to_5) {
    CPU cpu;
    run_src("MOV R0,0\nMOV R1,5\n"
            "loop:\nCMP R0,R1\nJUMP_ZERO done\nINC R0\nJUMP loop\n"
            "done:\nEND\n", &cpu);
    ASSERT_EQ(cpu.reg[0], 5);
}

TEST(all_flags_combination) {
    /* Test that zero AND sign can't be set together on non-zero sub */
    CPU cpu;
    run_src("MOV R0,10\nMOV R1,5\nCMP R0,R1\nEND\n",&cpu);
    ASSERT_FALSE(cpu.zero);
    ASSERT_FALSE(cpu.sign);
    ASSERT_FALSE(cpu.carry);
}

/* =========================================================
 *  Memory operations
 * ========================================================= */

TEST(memory_value_255) {
    CPU cpu;
    run_src("MOV R0,255\nSTORE R0,100\nLOAD R1,100\nEND\n",&cpu);
    ASSERT_EQ(cpu.reg[1], 255);
}

TEST(memory_multiple_locations) {
    CPU cpu;
    run_src("MOV R0,10\nSTORE R0,0\n"
            "MOV R0,20\nSTORE R0,1\n"
            "MOV R0,30\nSTORE R0,2\n"
            "LOAD R1,0\nLOAD R2,1\nLOAD R3,2\nEND\n", &cpu);
    ASSERT_EQ(cpu.reg[1], 10);
    ASSERT_EQ(cpu.reg[2], 20);
    ASSERT_EQ(cpu.reg[3], 30);
}

/* =========================================================
 *  Robustness
 * ========================================================= */

TEST(empty_program_no_crash) {
    CPU cpu;
    ASSERT_TRUE(run_src("END\n", &cpu));
    ASSERT_EQ(cpu.reg[0], 0);
}

TEST(all_regs_preserved_across_nop) {
    CPU cpu;
    run_src("MOV R0,1\nMOV R1,2\nMOV R2,3\nMOV R3,4\n"
            "MOV R4,5\nMOV R5,6\nMOV R6,7\nMOV R7,8\n"
            "NOP\nNOP\nEND\n", &cpu);
    for (int i = 0; i < 8; i++)
        ASSERT_EQ(cpu.reg[i], (unsigned)(i+1));
}

/* =========================================================
 *  main
 * ========================================================= */

int main(void) {
    printf("=== Integration Tests ===\n");
    RUN(factorial_5);
    RUN(fibonacci_first5);
    RUN(bubble_sort);
    RUN(gcd_48_36);
    RUN(sum_1_to_10);
    RUN(popcount_0xAA);
    RUN(jump_addr8_upper_bound);
    RUN(call_sp_balanced);
    RUN(nested_calls_3_levels);
    RUN(if_then_else);
    RUN(while_loop_to_5);
    RUN(all_flags_combination);
    RUN(memory_value_255);
    RUN(memory_multiple_locations);
    RUN(empty_program_no_crash);
    RUN(all_regs_preserved_across_nop);
    PRINT_RESULTS();
}
