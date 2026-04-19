/*
 * test_call_preservation.c — Register preservation across CALL/RET (C99)
 *
 * Covers every scenario where register values could be lost across a
 * function call boundary:
 *
 *   1.  Unmodified registers are preserved by the callee (untouched regs)
 *   2.  Caller-save pattern: PUSH before CALL, POP after RET
 *   3.  Callee-save pattern: function saves/restores registers it uses
 *   4.  NOP function: all 8 registers completely unchanged
 *   5.  Caller saves multiple registers around one call
 *   6.  Nested calls: registers survive multi-level call chains
 *   7.  Return value in R0: R1-R7 untouched
 *   8.  SP fully restored when callee uses PUSH/POP internally
 *   9.  Two consecutive calls: registers between calls are stable
 *  10.  Argument in R0, result in R0: R1-R7 unchanged
 *  11.  Callee saves three registers, caller's values untouched
 *  12.  A PUSH'd value survives a CALL/RET (stack not corrupted)
 *  13.  Deep 3-level nested call: R1,R2,R3 unchanged, only R0 set
 *  14.  Caller saves R0 across two consecutive calls
 *  15.  Boundary immediate values (0 and 255) survive a call
 *  16.  Mixed caller-save / callee-save cooperation
 *  17.  Function with an internal conditional branch: R2 unchanged either way
 *  18.  Function that writes to memory but not to any register
 *  19.  Two consecutive calls each with their own callee-save
 *  20.  Recursive countdown: stack balanced, R0 restored to original value
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
 *  Helper: assemble + run
 * ========================================================= */

static int run_src(const char *src, CPU *cpu) {
    Program p;
    ErrorList el;
    el.count = 0;
    if (!assemble(src, &p, &el) || el.count > 0) {
        if (el.count > 0) fprintf(stderr, "  ASM error: %s\n", el.msgs[0]);
        prog_free(&p);
        return 0;
    }
    cpu_reset(cpu);
    run_program(cpu, &p, 100000, 0);
    prog_free(&p);
    return 1;
}

/* =========================================================
 *  1. Unmodified registers are preserved by the callee
 *     Function only writes R0; R1-R7 must be exactly as
 *     the caller left them.
 * ========================================================= */

TEST(unmodified_regs_preserved) {
    CPU cpu;
    run_src(
        "MOV R1,11\n"
        "MOV R2,22\n"
        "MOV R3,33\n"
        "MOV R4,44\n"
        "MOV R5,55\n"
        "MOV R6,66\n"
        "MOV R7,77\n"
        "CALL f\n"
        "END\n"
        "f:\n"
        "MOV R0,99\n"
        "RET\n",
        &cpu);
    ASSERT_EQ(cpu.reg[0], 99);   /* callee wrote this */
    ASSERT_EQ(cpu.reg[1], 11);
    ASSERT_EQ(cpu.reg[2], 22);
    ASSERT_EQ(cpu.reg[3], 33);
    ASSERT_EQ(cpu.reg[4], 44);
    ASSERT_EQ(cpu.reg[5], 55);
    ASSERT_EQ(cpu.reg[6], 66);
    ASSERT_EQ(cpu.reg[7], 77);
}

/* =========================================================
 *  2. Caller-save pattern: PUSH before CALL, POP after RET
 *     Caller saves R0 on the stack; callee clobbers R0;
 *     after POP the caller's original value is back.
 * ========================================================= */

TEST(caller_save_restores_r0) {
    CPU cpu;
    run_src(
        "MOV R0,42\n"
        "PUSH R0\n"           /* save caller's R0 */
        "CALL clobber\n"
        "POP R0\n"            /* restore caller's R0 */
        "END\n"
        "clobber:\n"
        "MOV R0,99\n"
        "RET\n",
        &cpu);
    ASSERT_EQ(cpu.reg[0], 42);  /* restored, not the callee's 99 */
}

/* =========================================================
 *  3. NOP function: all 8 registers completely unchanged
 *     Call a function that does nothing but RET.
 * ========================================================= */

TEST(nop_function_all_regs_unchanged) {
    CPU cpu;
    run_src(
        "MOV R0,1\n"
        "MOV R1,2\n"
        "MOV R2,3\n"
        "MOV R3,4\n"
        "MOV R4,5\n"
        "MOV R5,6\n"
        "MOV R6,7\n"
        "MOV R7,8\n"
        "CALL nop_fn\n"
        "END\n"
        "nop_fn:\n"
        "RET\n",
        &cpu);
    for (int i = 0; i < 8; i++)
        ASSERT_EQ(cpu.reg[i], (unsigned)(i + 1));
}

/* =========================================================
 *  4. Callee-save pattern: function saves and restores R2
 *     Caller's R2 must be unchanged even though the function
 *     uses R2 internally.
 * ========================================================= */

TEST(callee_save_single_reg) {
    CPU cpu;
    run_src(
        "MOV R2,55\n"
        "CALL worker\n"
        "END\n"
        "worker:\n"
        "PUSH R2\n"         /* callee saves R2 */
        "MOV R2,99\n"       /* use R2 internally */
        "STORE R2,10\n"     /* write result to memory */
        "POP R2\n"          /* callee restores R2 */
        "RET\n",
        &cpu);
    ASSERT_EQ(cpu.reg[2], 55);   /* caller's R2 intact */
    ASSERT_EQ(cpu.mem[10], 99);  /* side-effect confirmed */
}

/* =========================================================
 *  5. Caller saves multiple registers (R1, R2, R3) around
 *     one call. The callee clobbers all three; after POP
 *     the originals are back (LIFO order).
 * ========================================================= */

TEST(caller_save_three_regs) {
    CPU cpu;
    run_src(
        "MOV R1,10\n"
        "MOV R2,20\n"
        "MOV R3,30\n"
        "PUSH R1\n"
        "PUSH R2\n"
        "PUSH R3\n"
        "CALL clobber3\n"
        "POP R3\n"          /* LIFO: restore in reverse order */
        "POP R2\n"
        "POP R1\n"
        "END\n"
        "clobber3:\n"
        "MOV R1,111\n"
        "MOV R2,222\n"
        "MOV R3,233\n"
        "RET\n",
        &cpu);
    ASSERT_EQ(cpu.reg[1], 10);
    ASSERT_EQ(cpu.reg[2], 20);
    ASSERT_EQ(cpu.reg[3], 30);
}

/* =========================================================
 *  6. Nested calls: register survives a 3-level call chain
 *     Main sets R3=77, then a → b → c executes.
 *     Only c touches R0; R3 must be 77 throughout.
 * ========================================================= */

TEST(three_level_nested_r3_preserved) {
    CPU cpu;
    run_src(
        "MOV R3,77\n"
        "CALL a\n"
        "END\n"
        "a:\n"
        "CALL b\n"
        "RET\n"
        "b:\n"
        "CALL c\n"
        "RET\n"
        "c:\n"
        "MOV R0,42\n"
        "RET\n",
        &cpu);
    ASSERT_EQ(cpu.reg[0], 42);
    ASSERT_EQ(cpu.reg[3], 77);
}

/* =========================================================
 *  7. Return value in R0: R1-R7 untouched
 *     Function increments R0; all other registers must
 *     still hold the values the caller set.
 * ========================================================= */

TEST(return_value_others_unchanged) {
    CPU cpu;
    run_src(
        "MOV R0,0\n"
        "MOV R1,11\n"
        "MOV R2,22\n"
        "MOV R3,33\n"
        "MOV R4,44\n"
        "MOV R5,55\n"
        "MOV R6,66\n"
        "MOV R7,77\n"
        "CALL inc_r0\n"
        "END\n"
        "inc_r0:\n"
        "INC R0\n"
        "RET\n",
        &cpu);
    ASSERT_EQ(cpu.reg[0], 1);
    ASSERT_EQ(cpu.reg[1], 11);
    ASSERT_EQ(cpu.reg[2], 22);
    ASSERT_EQ(cpu.reg[3], 33);
    ASSERT_EQ(cpu.reg[4], 44);
    ASSERT_EQ(cpu.reg[5], 55);
    ASSERT_EQ(cpu.reg[6], 66);
    ASSERT_EQ(cpu.reg[7], 77);
}

/* =========================================================
 *  8. SP fully restored when callee uses PUSH/POP internally
 *     Function pushes and pops two registers; after RET
 *     SP must be back to 0xFF.
 * ========================================================= */

TEST(sp_restored_after_callee_push_pop) {
    CPU cpu;
    cpu_reset(&cpu);
    Program p; ErrorList el; el.count = 0;
    assemble(
        "CALL f\n"
        "END\n"
        "f:\n"
        "PUSH R0\n"
        "PUSH R1\n"
        "MOV R0,42\n"
        "MOV R1,43\n"
        "POP R1\n"
        "POP R0\n"
        "RET\n",
        &p, &el);
    run_program(&cpu, &p, 100000, 0);
    ASSERT_EQ(cpu.sp, 0xFF);    /* stack fully balanced */
    ASSERT_EQ(cpu.reg[0], 0);   /* callee restored R0 to its original 0 */
    ASSERT_EQ(cpu.reg[1], 0);   /* callee restored R1 to its original 0 */
    prog_free(&p);
}

/* =========================================================
 *  9. Two consecutive calls: register between calls is stable
 *     R2=5 must survive both calls even though each call
 *     modifies R0 and R1.
 * ========================================================= */

TEST(two_consecutive_calls_r2_survives) {
    CPU cpu;
    run_src(
        "MOV R2,5\n"
        "CALL f1\n"
        "CALL f2\n"
        "END\n"
        "f1:\n"
        "MOV R0,10\n"
        "MOV R1,20\n"
        "RET\n"
        "f2:\n"
        "MOV R0,30\n"
        "MOV R1,40\n"
        "RET\n",
        &cpu);
    ASSERT_EQ(cpu.reg[2], 5);
}

/* =========================================================
 *  10. Argument in R0, result in R0: R1-R3 unchanged
 *      Function doubles R0; R1, R2, R3 must still hold
 *      the values the caller loaded.
 * ========================================================= */

TEST(r0_doubled_others_unchanged) {
    CPU cpu;
    run_src(
        "MOV R0,5\n"
        "MOV R1,11\n"
        "MOV R2,22\n"
        "MOV R3,33\n"
        "CALL dbl\n"
        "END\n"
        "dbl:\n"
        "ADD R0,R0\n"
        "RET\n",
        &cpu);
    ASSERT_EQ(cpu.reg[0], 10);
    ASSERT_EQ(cpu.reg[1], 11);
    ASSERT_EQ(cpu.reg[2], 22);
    ASSERT_EQ(cpu.reg[3], 33);
}

/* =========================================================
 *  11. Callee saves three registers, caller's values intact
 *      The function PUSHes R1, R2, R3 on entry, does some
 *      work with them, then POPs them before RET.
 * ========================================================= */

TEST(callee_saves_three_regs) {
    CPU cpu;
    run_src(
        "MOV R1,10\n"
        "MOV R2,20\n"
        "MOV R3,30\n"
        "CALL worker\n"
        "END\n"
        "worker:\n"
        "PUSH R1\n"         /* callee saves */
        "PUSH R2\n"
        "PUSH R3\n"
        "MOV R1,111\n"      /* use regs internally */
        "MOV R2,222\n"
        "MOV R3,233\n"
        "MOV R0,42\n"       /* return value */
        "POP R3\n"          /* callee restores (LIFO) */
        "POP R2\n"
        "POP R1\n"
        "RET\n",
        &cpu);
    ASSERT_EQ(cpu.reg[0], 42);   /* return value */
    ASSERT_EQ(cpu.reg[1], 10);   /* caller's R1 restored */
    ASSERT_EQ(cpu.reg[2], 20);   /* caller's R2 restored */
    ASSERT_EQ(cpu.reg[3], 30);   /* caller's R3 restored */
}

/* =========================================================
 *  12. A PUSH'd value survives a CALL/RET
 *      Push a value onto the stack, call a void function,
 *      pop the value: it must be exactly what was pushed.
 *      This verifies CALL/RET do not corrupt data on the stack.
 * ========================================================= */

TEST(push_survives_call) {
    CPU cpu;
    run_src(
        "MOV R0,99\n"
        "PUSH R0\n"
        "CALL do_nothing\n"
        "POP R1\n"          /* must recover 99 */
        "END\n"
        "do_nothing:\n"
        "RET\n",
        &cpu);
    ASSERT_EQ(cpu.reg[1], 99);
}

/* =========================================================
 *  13. Deep 3-level nested call: R1, R2, R3 unchanged
 *      None of the three chained functions touch R1-R3;
 *      only the leaf function (c) sets R0.
 * ========================================================= */

TEST(deep_nested_r1_r2_r3_preserved) {
    CPU cpu;
    run_src(
        "MOV R1,100\n"
        "MOV R2,200\n"
        "MOV R3,123\n"
        "CALL fn_a\n"
        "END\n"
        "fn_a:\n"
        "CALL fn_b\n"
        "RET\n"
        "fn_b:\n"
        "CALL fn_c\n"
        "RET\n"
        "fn_c:\n"
        "MOV R0,42\n"
        "RET\n",
        &cpu);
    ASSERT_EQ(cpu.reg[0], 42);
    ASSERT_EQ(cpu.reg[1], 100);
    ASSERT_EQ(cpu.reg[2], 200);
    ASSERT_EQ(cpu.reg[3], 123);
}

/* =========================================================
 *  14. Caller saves R0 across two consecutive calls
 *      PUSH before the first call, POP after the second.
 *      Both calls clobber R0 with different values;
 *      the caller must recover its original R0=42.
 * ========================================================= */

TEST(caller_saves_r0_across_two_calls) {
    CPU cpu;
    run_src(
        "MOV R0,42\n"
        "PUSH R0\n"
        "CALL clobber_a\n"  /* sets R0=11 */
        "CALL clobber_b\n"  /* sets R0=22 */
        "POP R0\n"          /* must get 42 back */
        "END\n"
        "clobber_a:\n"
        "MOV R0,11\n"
        "RET\n"
        "clobber_b:\n"
        "MOV R0,22\n"
        "RET\n",
        &cpu);
    ASSERT_EQ(cpu.reg[0], 42);
}

/* =========================================================
 *  15. Boundary immediate values (0 and 255) survive a call
 *      The minimum and maximum 8-bit values must come through
 *      a CALL/RET completely unchanged.
 * ========================================================= */

TEST(boundary_values_through_call) {
    CPU cpu;
    run_src(
        "MOV R0,0\n"
        "MOV R1,255\n"
        "CALL nop_fn\n"
        "END\n"
        "nop_fn:\n"
        "RET\n",
        &cpu);
    ASSERT_EQ(cpu.reg[0], 0);
    ASSERT_EQ(cpu.reg[1], 255);
}

/* =========================================================
 *  16. Mixed caller-save / callee-save cooperation
 *      - R1=10 is caller-save (caller PUSHes it because the
 *        callee will clobber it).
 *      - R2=55 is callee-save (callee PUSHes/POPs it itself).
 *      After the call: R0 = result (=10), R1 = 10 (caller
 *      restored it), R2 = 55 (callee restored it).
 * ========================================================= */

TEST(mixed_caller_callee_save) {
    CPU cpu;
    run_src(
        "MOV R1,10\n"
        "MOV R2,55\n"
        "PUSH R1\n"         /* caller saves R1 (callee clobbers it) */
        "CALL worker\n"
        "POP R1\n"          /* caller restores R1 */
        "END\n"
        "worker:\n"
        "PUSH R2\n"         /* callee saves R2 */
        "MOV R0,R1\n"       /* result = R1 (= 10) */
        "MOV R2,99\n"       /* internal use of R2 */
        "MOV R1,0\n"        /* clobber R1 — caller's job to restore */
        "POP R2\n"          /* callee restores R2 */
        "RET\n",
        &cpu);
    ASSERT_EQ(cpu.reg[0], 10);   /* return value */
    ASSERT_EQ(cpu.reg[1], 10);   /* caller-saved and restored */
    ASSERT_EQ(cpu.reg[2], 55);   /* callee-saved and restored */
}

/* =========================================================
 *  17. Function with an internal conditional branch
 *      R2=42 must be unchanged regardless of which branch
 *      the function takes.  With R0=5 > R1=3, JUMP_ZERO
 *      is not taken and INC R0 executes; R2 is never touched.
 * ========================================================= */

TEST(function_with_branch_preserves_r2) {
    CPU cpu;
    run_src(
        "MOV R0,5\n"
        "MOV R1,3\n"
        "MOV R2,42\n"
        "CALL branchy\n"
        "END\n"
        "branchy:\n"
        "CMP R0,R1\n"       /* 5-3=2, ZF=0 */
        "JUMP_ZERO skip\n"  /* not taken */
        "INC R0\n"          /* R0 becomes 6 */
        "skip:\n"
        "RET\n",
        &cpu);
    ASSERT_EQ(cpu.reg[0], 6);
    ASSERT_EQ(cpu.reg[2], 42);   /* untouched by either branch */
}

/* And the not-taken branch of JUMP_ZERO: R0==R1 → ZF=1 → skip INC */
TEST(function_branch_not_taken_preserves_r2) {
    CPU cpu;
    run_src(
        "MOV R0,5\n"
        "MOV R1,5\n"
        "MOV R2,42\n"
        "CALL branchy\n"
        "END\n"
        "branchy:\n"
        "CMP R0,R1\n"       /* 5-5=0, ZF=1 */
        "JUMP_ZERO skip\n"  /* taken — skip the INC */
        "INC R0\n"
        "skip:\n"
        "RET\n",
        &cpu);
    ASSERT_EQ(cpu.reg[0], 5);    /* INC was skipped */
    ASSERT_EQ(cpu.reg[2], 42);   /* untouched */
}

/* =========================================================
 *  18. Function writes to memory but not to any register
 *      STORE does not modify registers; R0 and R1 must be
 *      unchanged after the call.
 * ========================================================= */

TEST(function_stores_memory_no_reg_side_effect) {
    CPU cpu;
    run_src(
        "MOV R0,123\n"
        "MOV R1,45\n"
        "CALL storer\n"
        "END\n"
        "storer:\n"
        "STORE R0,20\n"     /* mem[20] = R0; R0 itself unchanged */
        "RET\n",
        &cpu);
    ASSERT_EQ(cpu.reg[0], 123);
    ASSERT_EQ(cpu.reg[1], 45);
    ASSERT_EQ(cpu.mem[20], 123);
}

/* =========================================================
 *  19. Two consecutive calls each with their own callee-save
 *      f1 saves R1 before using it; f2 saves R2 before using
 *      it. After both calls the caller's R1=10 and R2=20
 *      must be intact.
 * ========================================================= */

TEST(two_consecutive_callee_saves) {
    CPU cpu;
    run_src(
        "MOV R1,10\n"
        "MOV R2,20\n"
        "CALL f1\n"
        "CALL f2\n"
        "END\n"
        "f1:\n"
        "PUSH R1\n"
        "MOV R1,111\n"
        "STORE R1,30\n"
        "POP R1\n"
        "RET\n"
        "f2:\n"
        "PUSH R2\n"
        "MOV R2,222\n"
        "STORE R2,31\n"
        "POP R2\n"
        "RET\n",
        &cpu);
    ASSERT_EQ(cpu.reg[1], 10);
    ASSERT_EQ(cpu.reg[2], 20);
    ASSERT_EQ(cpu.mem[30], 111);  /* f1's side effect confirmed */
    ASSERT_EQ(cpu.mem[31], 222);  /* f2's side effect confirmed */
}

/* =========================================================
 *  20. Recursive countdown: stack balanced, R0 restored
 *      countdown(n) saves n on the stack, recurses with n-1,
 *      then restores n. After the top-level call returns,
 *      R0 must equal its original value and SP must be 0xFF.
 *
 *      Assembly layout:
 *        0: MOV R0,3
 *        1: CALL countdown
 *        2: END
 *        3: MOV R1,0      ← countdown start
 *        4: CMP R0,R1
 *        5: JUMP_ZERO done (→ 9)
 *        6: DEC R0
 *        7: CALL countdown (→ 3)
 *        8: INC R0
 *        9: RET           ← done
 * ========================================================= */

TEST(recursive_countdown_stack_balanced) {
    CPU cpu;
    cpu_reset(&cpu);
    Program p; ErrorList el; el.count = 0;
    assemble(
        "MOV R0,3\n"
        "CALL countdown\n"
        "END\n"
        "countdown:\n"
        "MOV R1,0\n"
        "CMP R0,R1\n"
        "JUMP_ZERO done\n"
        "DEC R0\n"
        "CALL countdown\n"
        "INC R0\n"
        "done:\n"
        "RET\n",
        &p, &el);
    ASSERT_EQ(el.count, 0);      /* assembly must succeed */
    run_program(&cpu, &p, 100000, 0);
    ASSERT_EQ(cpu.reg[0], 3);    /* R0 fully restored */
    ASSERT_EQ(cpu.sp, 0xFF);     /* stack perfectly balanced */
    prog_free(&p);
}

/* =========================================================
 *  main
 * ========================================================= */

int main(void) {
    printf("=== Call / Register Preservation Tests ===\n");

    RUN(unmodified_regs_preserved);
    RUN(caller_save_restores_r0);
    RUN(nop_function_all_regs_unchanged);
    RUN(callee_save_single_reg);
    RUN(caller_save_three_regs);
    RUN(three_level_nested_r3_preserved);
    RUN(return_value_others_unchanged);
    RUN(sp_restored_after_callee_push_pop);
    RUN(two_consecutive_calls_r2_survives);
    RUN(r0_doubled_others_unchanged);
    RUN(callee_saves_three_regs);
    RUN(push_survives_call);
    RUN(deep_nested_r1_r2_r3_preserved);
    RUN(caller_saves_r0_across_two_calls);
    RUN(boundary_values_through_call);
    RUN(mixed_caller_callee_save);
    RUN(function_with_branch_preserves_r2);
    RUN(function_branch_not_taken_preserves_r2);
    RUN(function_stores_memory_no_reg_side_effect);
    RUN(two_consecutive_callee_saves);
    RUN(recursive_countdown_stack_balanced);

    PRINT_RESULTS();
}
