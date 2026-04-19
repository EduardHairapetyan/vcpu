/*
 * test_emulator.c — Exhaustive unit tests for the vCPU emulator (C99)
 *
 * Coverage targets beyond previous suite:
 *   - print_state() (debug=1 path)               [emulator.c L22-32]
 *   - stack_pop underflow path                   [emulator.c L61-62]
 *   - execute_one debug printf path              [emulator.c L87]
 *   - JUMP_SIGNED, JUMP_NOT_SIGNED               [emulator.c L130-131]
 *   - JUMP_CARRY, JUMP_NOT_CARRY                 [emulator.c L132-133]
 *   - Unknown opcode default path                [emulator.c L187-189]
 *   - All six conditional jumps both taken/not-taken
 */

#ifndef VCPU_TESTING
#define VCPU_TESTING
#endif
#include "test_framework.h"
#include "vcpu.h"
#include "assembler.h"
#include "assembler.c"
#include "emulator.c"

/* =========================================================
 *  Helper: assemble + run, return 1 on success
 * ========================================================= */

static int run_src(const char *src, CPU *cpu) {
    Program p;
    ErrorList el;
    el.count = 0;
    if (!assemble(src, &p, &el) || el.count > 0) {
        if (el.count > 0) fprintf(stderr, "  ASM: %s\n", el.msgs[0]);
        prog_free(&p);
        return 0;
    }
    cpu_reset(cpu);
    run_program(cpu, &p, 100000, 0);
    prog_free(&p);
    return 1;
}

/* =========================================================
 *  Arithmetic and flag tests
 * ========================================================= */

TEST(add_basic) {
    CPU cpu;
    run_src("MOV R0,10\nMOV R1,20\nADD R0,R1\nEND\n", &cpu);
    ASSERT_EQ(cpu.reg[0], 30);
}
TEST(sub_basic) {
    CPU cpu;
    run_src("MOV R0,50\nMOV R1,20\nSUB R0,R1\nEND\n", &cpu);
    ASSERT_EQ(cpu.reg[0], 30);
}
TEST(add_carry_flag) {
    CPU cpu;
    run_src("MOV R0,200\nMOV R1,100\nADD R0,R1\nEND\n", &cpu);
    ASSERT_EQ(cpu.reg[0], 44);
    ASSERT_TRUE(cpu.carry);
}
TEST(add_zero_flag) {
    CPU cpu;
    run_src("MOV R0,0\nMOV R1,0\nADD R0,R1\nEND\n", &cpu);
    ASSERT_TRUE(cpu.zero);
    ASSERT_FALSE(cpu.sign);
    ASSERT_FALSE(cpu.carry);
}
TEST(sub_sign_flag) {
    CPU cpu;
    run_src("MOV R0,5\nMOV R1,10\nSUB R0,R1\nEND\n", &cpu);
    ASSERT_TRUE(cpu.sign);
    ASSERT_FALSE(cpu.zero);
}
TEST(and_basic)  { CPU c; run_src("MOV R0,0xFF\nMOV R1,0x0F\nAND R0,R1\nEND\n",&c); ASSERT_EQ(c.reg[0],0x0F); }
TEST(or_basic)   { CPU c; run_src("MOV R0,0xF0\nMOV R1,0x0F\nOR R0,R1\nEND\n", &c); ASSERT_EQ(c.reg[0],0xFF); }
TEST(xor_basic)  { CPU c; run_src("MOV R0,0xFF\nMOV R1,0xFF\nXOR R0,R1\nEND\n",&c); ASSERT_EQ(c.reg[0],0); ASSERT_TRUE(c.zero); }
TEST(cmp_equal)  { CPU c; run_src("MOV R0,5\nMOV R1,5\nCMP R0,R1\nEND\n", &c);  ASSERT_TRUE(c.zero); ASSERT_EQ(c.reg[0],5); }
TEST(cmp_greater){ CPU c; run_src("MOV R0,10\nMOV R1,5\nCMP R0,R1\nEND\n",&c); ASSERT_FALSE(c.zero); ASSERT_FALSE(c.sign); }
TEST(inc_dec)    { CPU c; run_src("MOV R0,5\nINC R0\nINC R0\nDEC R0\nEND\n",&c); ASSERT_EQ(c.reg[0],6); }
TEST(inc_wrap)   { CPU c; run_src("MOV R0,255\nINC R0\nEND\n",&c); ASSERT_EQ(c.reg[0],0); ASSERT_TRUE(c.zero); ASSERT_TRUE(c.carry); }
TEST(dec_zero_flag){ CPU c; run_src("MOV R0,1\nDEC R0\nEND\n",&c); ASSERT_EQ(c.reg[0],0); ASSERT_TRUE(c.zero); }

/* =========================================================
 *  MOV variants
 * ========================================================= */

TEST(mov_reg_reg) { CPU c; run_src("MOV R0,77\nMOV R1,R0\nEND\n",&c); ASSERT_EQ(c.reg[1],77); }

TEST(mov_imm_all_regs) {
    CPU cpu;
    run_src("MOV R0,1\nMOV R1,2\nMOV R2,3\nMOV R3,4\n"
            "MOV R4,5\nMOV R5,6\nMOV R6,7\nMOV R7,8\nEND\n", &cpu);
    for (int i = 0; i < 8; i++)
        ASSERT_EQ(cpu.reg[i], (unsigned)(i+1));
}

/* =========================================================
 *  Memory: LOAD / STORE
 * ========================================================= */

TEST(store_load_roundtrip) {
    CPU cpu;
    run_src("MOV R0,123\nSTORE R0,50\nMOV R0,0\nLOAD R0,50\nEND\n",&cpu);
    ASSERT_EQ(cpu.reg[0],123);
}
TEST(store_no_flag_change) {
    CPU cpu;
    run_src("MOV R0,5\nMOV R1,0\nCMP R0,R1\nSTORE R0,10\nEND\n",&cpu);
    ASSERT_FALSE(cpu.zero);
}

/* =========================================================
 *  Conditional jumps — all six, both taken and not-taken
 * ========================================================= */

TEST(jump_unconditional) {
    CPU cpu;
    run_src("JUMP done\nMOV R0,99\ndone:\nEND\n",&cpu);
    ASSERT_EQ(cpu.reg[0],0);
}

/* JUMP_ZERO */
TEST(jump_zero_taken)     { CPU c; run_src("MOV R0,5\nMOV R1,5\nCMP R0,R1\nJUMP_ZERO ok\nMOV R0,99\nok:\nEND\n",&c); ASSERT_EQ(c.reg[0],5); }
TEST(jump_zero_not_taken) { CPU c; run_src("MOV R0,5\nMOV R1,3\nCMP R0,R1\nJUMP_ZERO ok\nMOV R0,99\nok:\nEND\n",&c); ASSERT_EQ(c.reg[0],99); }

/* JUMP_NOT_ZERO */
TEST(jump_nz_taken)     { CPU c; run_src("MOV R0,5\nMOV R1,3\nCMP R0,R1\nJUMP_NOT_ZERO ok\nMOV R0,99\nok:\nEND\n",&c); ASSERT_EQ(c.reg[0],5); }
TEST(jump_nz_not_taken) { CPU c; run_src("MOV R0,5\nMOV R1,5\nCMP R0,R1\nJUMP_NOT_ZERO ok\nMOV R0,99\nok:\nEND\n",&c); ASSERT_EQ(c.reg[0],99); }

/* JUMP_SIGNED — exercises previously uncovered path */
TEST(jump_signed_taken) {
    /* 5 - 10 → result 0xFB, sign flag set → JUMP_SIGNED is taken */
    CPU cpu;
    run_src("MOV R0,5\nMOV R1,10\nCMP R0,R1\nJUMP_SIGNED ok\nMOV R0,99\nok:\nEND\n",&cpu);
    ASSERT_EQ(cpu.reg[0],5);
}
TEST(jump_signed_not_taken) {
    /* 10 - 5 → positive, sign flag clear → JUMP_SIGNED not taken */
    CPU cpu;
    run_src("MOV R0,10\nMOV R1,5\nCMP R0,R1\nJUMP_SIGNED ok\nMOV R0,99\nok:\nEND\n",&cpu);
    ASSERT_EQ(cpu.reg[0],99);
}

/* JUMP_NOT_SIGNED — exercises previously uncovered path */
TEST(jump_not_signed_taken) {
    CPU cpu;
    run_src("MOV R0,10\nMOV R1,5\nCMP R0,R1\nJUMP_NOT_SIGNED ok\nMOV R0,99\nok:\nEND\n",&cpu);
    ASSERT_EQ(cpu.reg[0],10);
}
TEST(jump_not_signed_not_taken) {
    CPU cpu;
    run_src("MOV R0,5\nMOV R1,10\nCMP R0,R1\nJUMP_NOT_SIGNED ok\nMOV R0,99\nok:\nEND\n",&cpu);
    ASSERT_EQ(cpu.reg[0],99);
}

/* JUMP_CARRY — exercises previously uncovered path */
TEST(jump_carry_taken) {
    /* 200 + 100 = 300, carry set */
    CPU cpu;
    run_src("MOV R0,200\nMOV R1,100\nADD R0,R1\nJUMP_CARRY ok\nMOV R0,99\nok:\nEND\n",&cpu);
    ASSERT_EQ(cpu.reg[0],44);
}
TEST(jump_carry_not_taken) {
    CPU cpu;
    run_src("MOV R0,1\nMOV R1,1\nADD R0,R1\nJUMP_CARRY ok\nMOV R0,99\nok:\nEND\n",&cpu);
    ASSERT_EQ(cpu.reg[0],99);
}

/* JUMP_NOT_CARRY — exercises previously uncovered path */
TEST(jump_not_carry_taken) {
    CPU cpu;
    run_src("MOV R0,1\nMOV R1,1\nADD R0,R1\nJUMP_NOT_CARRY ok\nMOV R0,99\nok:\nEND\n",&cpu);
    ASSERT_EQ(cpu.reg[0],2);
}
TEST(jump_not_carry_not_taken) {
    CPU cpu;
    run_src("MOV R0,200\nMOV R1,100\nADD R0,R1\nJUMP_NOT_CARRY ok\nMOV R0,99\nok:\nEND\n",&cpu);
    ASSERT_EQ(cpu.reg[0],99);
}

/* =========================================================
 *  Loop
 * ========================================================= */

TEST(loop_count) {
    CPU cpu;
    run_src("MOV R0,0\nMOV R1,5\n"
            "loop:\nINC R0\nCMP R0,R1\nJUMP_NOT_ZERO loop\nEND\n",&cpu);
    ASSERT_EQ(cpu.reg[0],5);
}

/* =========================================================
 *  Stack: PUSH / POP
 * ========================================================= */

TEST(push_pop_basic) {
    CPU cpu;
    run_src("MOV R0,42\nPUSH R0\nMOV R0,0\nPOP R0\nEND\n",&cpu);
    ASSERT_EQ(cpu.reg[0],42);
}
TEST(stack_grows_down) {
    CPU cpu; cpu_reset(&cpu);
    Program p; ErrorList el; el.count=0;
    assemble("MOV R0,1\nPUSH R0\nMOV R0,2\nPUSH R0\nEND\n",&p,&el);
    run_program(&cpu,&p,1000,0);
    ASSERT_EQ(cpu.sp,(unsigned)(0xFF-2));
    prog_free(&p);
}
TEST(push_pop_lifo) {
    CPU cpu;
    run_src("MOV R0,10\nMOV R1,20\nPUSH R0\nPUSH R1\nPOP R2\nPOP R3\nEND\n",&cpu);
    ASSERT_EQ(cpu.reg[2],20);
    ASSERT_EQ(cpu.reg[3],10);
}

/* =========================================================
 *  CALL / RET
 * ========================================================= */

TEST(call_ret_basic) {
    CPU cpu;
    run_src("MOV R0,0\nCALL f\nEND\nf:\nMOV R0,42\nRET\n",&cpu);
    ASSERT_EQ(cpu.reg[0],42);
}
TEST(call_sp_delta_one) {
    CPU cpu; cpu_reset(&cpu);
    Program p; ErrorList el; el.count=0;
    assemble("CALL f\nEND\nf:\nRET\n",&p,&el);
    run_program(&cpu,&p,1000,0);
    ASSERT_EQ(cpu.sp,0xFF);
    prog_free(&p);
}
TEST(call_ret_value) {
    CPU cpu;
    run_src("MOV R0,5\nCALL dbl\nEND\ndbl:\nADD R0,R0\nRET\n",&cpu);
    ASSERT_EQ(cpu.reg[0],10);
}
TEST(nested_call_ret) {
    CPU cpu;
    run_src("CALL a\nEND\na:\nCALL b\nRET\nb:\nCALL c\nRET\nc:\nMOV R0,77\nRET\n",&cpu);
    ASSERT_EQ(cpu.reg[0],77);
    ASSERT_EQ(cpu.sp,0xFF);
}

/* =========================================================
 *  NOP, END
 * ========================================================= */

TEST(nop_does_nothing) {
    CPU cpu;
    run_src("MOV R0,5\nNOP\nNOP\nEND\n",&cpu);
    ASSERT_EQ(cpu.reg[0],5);
}

/* =========================================================
 *  Halt / error paths — previously uncovered
 * ========================================================= */

TEST(stack_overflow_halts) {
    CPU cpu; cpu_reset(&cpu);
    Program p; ErrorList el; el.count=0;
    assemble("loop:\nCALL loop\n",&p,&el);
    int steps = run_program(&cpu,&p,300,0);
    ASSERT_TRUE(steps < 300);
    prog_free(&p);
}

TEST(stack_underflow_halts) {
    /* POP on empty stack (SP == 0xFF) exercises stack_pop underflow path */
    CPU cpu; cpu_reset(&cpu);
    /* SP starts at 0xFF — pop immediately */
    Program p; ErrorList el; el.count=0;
    assemble("POP R0\nEND\n",&p,&el);
    int steps = run_program(&cpu,&p,10,0);
    /* Should halt immediately on the underflow */
    ASSERT_TRUE(steps <= 1);
    prog_free(&p);
}

TEST(ip_out_of_bounds_halts) {
    CPU cpu; cpu_reset(&cpu);
    Program p; ErrorList el; el.count=0;
    assemble("NOP\nEND\n",&p,&el);
    cpu.ip = (uint16_t)p.size;
    int result = execute_one(&cpu,&p,0);
    ASSERT_EQ(result,0);
    prog_free(&p);
}

TEST(unknown_opcode_halts) {
    /* Manually construct a word with the reserved opcode 0x1F */
    CPU cpu; cpu_reset(&cpu);
    Program p; prog_init(&p);
    prog_push(&p, (uint16_t)(0x1F << 11));   /* opcode 31, reserved */
    int result = execute_one(&cpu, &p, 0);
    ASSERT_EQ(result, 0);
    prog_free(&p);
}

/* =========================================================
 *  Debug mode paths — exercises print_state and debug printf
 * ========================================================= */

TEST(debug_mode_no_crash) {
    /* run_program with debug=1 exercises print_state() and the
       "[IP=...] 0x%04X ..." debug printf in execute_one() */
    CPU cpu; cpu_reset(&cpu);
    Program p; ErrorList el; el.count=0;
    assemble("MOV R0,5\nEND\n",&p,&el);
    run_program(&cpu,&p,10,1);   /* debug=1 */
    prog_free(&p);
    ASSERT_EQ(cpu.reg[0],5);
}

/* =========================================================
 *  main
 * ========================================================= */

int main(void) {
    printf("=== Emulator Tests ===\n");
    RUN(add_basic); RUN(sub_basic);
    RUN(add_carry_flag); RUN(add_zero_flag); RUN(sub_sign_flag);
    RUN(and_basic); RUN(or_basic); RUN(xor_basic);
    RUN(cmp_equal); RUN(cmp_greater);
    RUN(inc_dec); RUN(inc_wrap); RUN(dec_zero_flag);

    RUN(mov_reg_reg); RUN(mov_imm_all_regs);
    RUN(store_load_roundtrip); RUN(store_no_flag_change);

    RUN(jump_unconditional);
    RUN(jump_zero_taken);      RUN(jump_zero_not_taken);
    RUN(jump_nz_taken);        RUN(jump_nz_not_taken);
    RUN(jump_signed_taken);    RUN(jump_signed_not_taken);
    RUN(jump_not_signed_taken);RUN(jump_not_signed_not_taken);
    RUN(jump_carry_taken);     RUN(jump_carry_not_taken);
    RUN(jump_not_carry_taken); RUN(jump_not_carry_not_taken);
    RUN(loop_count);

    RUN(push_pop_basic); RUN(stack_grows_down); RUN(push_pop_lifo);
    RUN(call_ret_basic); RUN(call_sp_delta_one);
    RUN(call_ret_value); RUN(nested_call_ret);
    RUN(nop_does_nothing);

    RUN(stack_overflow_halts);
    RUN(stack_underflow_halts);
    RUN(ip_out_of_bounds_halts);
    RUN(unknown_opcode_halts);
    RUN(debug_mode_no_crash);

    PRINT_RESULTS();
}
