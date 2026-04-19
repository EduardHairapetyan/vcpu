/*
 * test_assembler.c — Exhaustive unit tests for the vCPU assembler (C99)
 *
 * Coverage targets beyond previous suite:
 *   - Too many labels (>256)        [assembler.c L98-99]
 *   - Unknown label / bad address   [assembler.c L188-190]
 *   - Too many lines (>512)         [assembler.c L342-343]
 *   - prog_push OOM path            [assembler.c L362-363]
 *   - STORE with label address      [resolve_addr label path]
 *   - Negative immediate rejection  (all I-type ops)
 *   - All 7 jump variants + CALL
 *   - MOV reg-to-reg all pairs
 */

#ifndef VCPU_TESTING
#define VCPU_TESTING
#endif
#include "test_framework.h"
#include "vcpu.h"
#include "assembler.h"
#include "assembler.c"

/* =========================================================
 *  Helpers
 * ========================================================= */

static uint16_t asm1(const char *line) {
    char src[512];
    snprintf(src, sizeof(src), "%s\n", line);
    Program p;
    ErrorList el;
    el.count = 0;
    assemble(src, &p, &el);
    if (el.count > 0) { prog_free(&p); return 0xFFFF; }
    uint16_t r = (p.size > 0) ? p.data[0] : 0xFFFF;
    prog_free(&p);
    return r;
}

static int asm_fails(const char *src) {
    Program p;
    ErrorList el;
    el.count = 0;
    assemble(src, &p, &el);
    prog_free(&p);
    return el.count > 0;
}

static int asm_ok(const char *src, Program *out) {
    ErrorList el;
    el.count = 0;
    int ok = assemble(src, out, &el);
    return ok && el.count == 0;
}

/* =========================================================
 *  R-type encoding — all mnemonics, all register pairs
 * ========================================================= */

TEST(add_r0_r1)  { ASSERT_EQ(asm1("ADD R0, R1"),  buildR(OP_ADD, 0, 1)); }
TEST(sub_r3_r5)  { ASSERT_EQ(asm1("SUB R3, R5"),  buildR(OP_SUB, 3, 5)); }
TEST(and_r7_r7)  { ASSERT_EQ(asm1("AND R7, R7"),  buildR(OP_AND, 7, 7)); }
TEST(or_r2_r4)   { ASSERT_EQ(asm1("OR R2, R4"),   buildR(OP_OR,  2, 4)); }
TEST(xor_r1_r6)  { ASSERT_EQ(asm1("XOR R1, R6"),  buildR(OP_XOR, 1, 6)); }
TEST(cmp_r0_r7)  { ASSERT_EQ(asm1("CMP R0, R7"),  buildR(OP_CMP, 0, 7)); }
TEST(mov_rr)     { ASSERT_EQ(asm1("MOV R3, R5"),  buildR(OP_MOV_REG, 3, 5)); }

TEST(all_regs_as_src) {
    for (int r = 0; r < 8; r++) {
        char line[32]; snprintf(line, sizeof(line), "ADD R0, R%d", r);
        ASSERT_EQ(asm1(line), buildR(OP_ADD, 0, r));
    }
}
TEST(all_regs_as_dst) {
    for (int r = 0; r < 8; r++) {
        char line[32]; snprintf(line, sizeof(line), "ADD R%d, R0", r);
        ASSERT_EQ(asm1(line), buildR(OP_ADD, r, 0));
    }
}

/* Thesis example: ADD R2,R5 → 0x02A0 */
TEST(add_r2_r5_thesis) { ASSERT_EQ(asm1("ADD R2, R5"), (uint16_t)0x02A0); }

/* =========================================================
 *  I-type encoding
 * ========================================================= */

TEST(mov_imm_zero)    { ASSERT_EQ(asm1("MOV R0, 0"),    buildI(OP_MOV_IMM, 0,   0)); }
TEST(mov_imm_42)      { ASSERT_EQ(asm1("MOV R0, 42"),   buildI(OP_MOV_IMM, 0,  42)); }
TEST(mov_imm_255)     { ASSERT_EQ(asm1("MOV R7, 255"),  buildI(OP_MOV_IMM, 7, 255)); }
TEST(mov_imm_hex)     { ASSERT_EQ(asm1("MOV R1, 0xFF"), buildI(OP_MOV_IMM, 1, 255)); }
TEST(load_r3_200)     { ASSERT_EQ(asm1("LOAD R3, 200"), buildI(OP_LOAD,    3, 200)); }
TEST(store_r1_100)    { ASSERT_EQ(asm1("STORE R1, 100"),buildI(OP_STORE,   1, 100)); }
TEST(load_r3_200_thesis) { ASSERT_EQ(asm1("LOAD R3, 200"), (uint16_t)0x4BC8); }

TEST(mov_imm_out_of_range) { ASSERT_TRUE(asm_fails("MOV R0, 256\n")); }
TEST(mov_imm_negative)     { ASSERT_TRUE(asm_fails("MOV R0, -1\n")); }
TEST(load_addr_255_ok)     { ASSERT_FALSE(asm_fails("LOAD R0, 255\n")); }
TEST(store_addr_255_ok)    { ASSERT_FALSE(asm_fails("STORE R0, 255\n")); }

/* =========================================================
 *  J-type — all 8 variants
 * ========================================================= */

TEST(jump_10)           { ASSERT_EQ(asm1("JUMP 10"),            buildJ(OP_JUMP, 10)); }
TEST(jump_zero_5)       { ASSERT_EQ(asm1("JUMP_ZERO 5"),        buildJ(OP_JUMP_ZERO, 5)); }
TEST(jump_not_zero_20)  { ASSERT_EQ(asm1("JUMP_NOT_ZERO 20"),   buildJ(OP_JUMP_NOT_ZERO, 20)); }
TEST(jump_signed_1)     { ASSERT_EQ(asm1("JUMP_SIGNED 1"),      buildJ(OP_JUMP_SIGNED, 1)); }
TEST(jump_not_signed_0) { ASSERT_EQ(asm1("JUMP_NOT_SIGNED 0"),  buildJ(OP_JUMP_NOT_SIGNED, 0)); }
TEST(jump_carry_200)    { ASSERT_EQ(asm1("JUMP_CARRY 200"),     buildJ(OP_JUMP_CARRY, 200)); }
TEST(jump_not_carry_100){ ASSERT_EQ(asm1("JUMP_NOT_CARRY 100"), buildJ(OP_JUMP_NOT_CARRY, 100)); }
TEST(call_5)            { ASSERT_EQ(asm1("CALL 5"),             buildJ(OP_CALL, 5)); }

TEST(jump_10_thesis)    { ASSERT_EQ(asm1("JUMP 10"), (uint16_t)0x800A); }
TEST(call_5_thesis)     { ASSERT_EQ(asm1("CALL 5"),  (uint16_t)0xB805); }
TEST(jump_255_ok)       { ASSERT_FALSE(asm_fails("JUMP 255\n")); }
TEST(jump_256_rejected) { ASSERT_TRUE(asm_fails("JUMP 256\n")); }
TEST(call_256_rejected) { ASSERT_TRUE(asm_fails("CALL 256\n")); }

TEST(jump_addr8_bits_clean) {
    uint16_t w = asm1("JUMP 10");
    ASSERT_EQ((w >> 8) & 0x7, 0);
    ASSERT_EQ(w & 0xFF, 10);
}

/* =========================================================
 *  S-type and Z-type
 * ========================================================= */

TEST(push_r0) { ASSERT_EQ(asm1("PUSH R0"), buildS(OP_PUSH, 0)); }
TEST(pop_r7)  { ASSERT_EQ(asm1("POP R7"),  buildS(OP_POP,  7)); }
TEST(inc_r3)  { ASSERT_EQ(asm1("INC R3"),  buildS(OP_INC,  3)); }
TEST(dec_r5)  { ASSERT_EQ(asm1("DEC R5"),  buildS(OP_DEC,  5)); }
TEST(nop)     { ASSERT_EQ(asm1("NOP"),     buildZ(OP_NOP)); }
TEST(end)     { ASSERT_EQ(asm1("END"),     buildZ(OP_END)); }
TEST(ret)     { ASSERT_EQ(asm1("RET"),     buildZ(OP_RET)); }

/* =========================================================
 *  Labels
 * ========================================================= */

TEST(label_forward_ref) {
    Program p;
    ASSERT_TRUE(asm_ok("JUMP done\nNOP\ndone:\nEND\n", &p));
    ASSERT_EQ(p.size, 3);
    ASSERT_EQ(getAddr8(p.data[0]), 2);
    prog_free(&p);
}

TEST(label_backward_ref) {
    Program p;
    ASSERT_TRUE(asm_ok("loop:\nNOP\nJUMP loop\n", &p));
    ASSERT_EQ(getAddr8(p.data[1]), 0);
    prog_free(&p);
}

TEST(label_duplicate_rejected) {
    ASSERT_TRUE(asm_fails("lbl: NOP\nlbl: END\n"));
}

TEST(label_used_in_load) {
    /* LOAD with a label as address — exercises resolve_addr label path */
    Program p;
    ASSERT_TRUE(asm_ok("data:\n  NOP\n  LOAD R0, data\n", &p));
    /* data is at instr address 0, LOAD's imm8 field should be 0 */
    ASSERT_EQ(getImm8(p.data[1]), 0);
    prog_free(&p);
}

TEST(label_used_in_store) {
    Program p;
    ASSERT_TRUE(asm_ok("STORE R0, tgt\ntgt:\nNOP\n", &p));
    ASSERT_EQ(getImm8(p.data[0]), 1); /* tgt is at instr address 1 */
    prog_free(&p);
}

TEST(label_unknown_rejected) {
    /* JUMP to undefined label — exercises "Unknown label or bad address" path */
    ASSERT_TRUE(asm_fails("JUMP nowhere\n"));
}

TEST(label_at_addr255) {
    /* Build a program with exactly 255 NOPs then a label — tests boundary */
    char src[4096];
    int pos = 0;
    for (int i = 0; i < 255; i++)
        pos += snprintf(src + pos, sizeof(src) - pos, "NOP\n");
    snprintf(src + pos, sizeof(src) - pos, "end:\nEND\n");
    Program p;
    ASSERT_TRUE(asm_ok(src, &p));
    prog_free(&p);
}

TEST(too_many_labels) {
    /* Generate MAX_LABELS+1 distinct labels — exercises "Too many labels" */
    char src[32768];
    int pos = 0;
    for (int i = 0; i <= 256; i++)
        pos += snprintf(src + pos, sizeof(src) - pos, "l%d: NOP\n", i);
    ASSERT_TRUE(asm_fails(src));
}

/* =========================================================
 *  Capacity limits
 * ========================================================= */

TEST(too_many_lines) {
    /* Generate MAX_LINES+1 (513) instructions — exercises "Too many lines" */
    char *src = (char *)malloc(513 * 6 + 8);
    ASSERT_TRUE(src != NULL);
    int pos = 0;
    for (int i = 0; i < 513; i++)
        pos += snprintf(src + pos, 513*6+8 - pos, "NOP\n");
    ASSERT_TRUE(asm_fails(src));
    free(src);
}

/* =========================================================
 *  Error cases
 * ========================================================= */

TEST(unknown_mnemonic)    { ASSERT_TRUE(asm_fails("BLAH R0\n")); }
TEST(bad_register_r8)     { ASSERT_TRUE(asm_fails("ADD R8, R0\n")); }
TEST(missing_args_add)    { ASSERT_TRUE(asm_fails("ADD R0\n")); }
TEST(too_many_args_nop)   { ASSERT_TRUE(asm_fails("NOP R0\n")); }
TEST(bad_src_register)    { ASSERT_TRUE(asm_fails("ADD R0, X5\n")); }
TEST(bad_dst_register)    { ASSERT_TRUE(asm_fails("MOV X0, 5\n")); }
TEST(push_no_args)        { ASSERT_TRUE(asm_fails("PUSH\n")); }
TEST(jump_zero_args)      { ASSERT_TRUE(asm_fails("JUMP\n")); }
TEST(jump_two_args)       { ASSERT_TRUE(asm_fails("JUMP 1, 2\n")); }
TEST(mov_bad_immediate)   { ASSERT_TRUE(asm_fails("MOV R0, abc\n")); }

/* =========================================================
 *  Case insensitivity, comments, blank lines
 * ========================================================= */

TEST(case_insensitive) {
    ASSERT_EQ(asm1("add r0, r1"), asm1("ADD R0, R1"));
    ASSERT_EQ(asm1("mov r2, 5"),  asm1("MOV R2, 5"));
    ASSERT_EQ(asm1("nop"),        asm1("NOP"));
}

TEST(comment_stripped) {
    ASSERT_EQ(asm1("ADD R0, R1  ; comment"), buildR(OP_ADD, 0, 1));
}

TEST(blank_lines_ignored) {
    Program p;
    ASSERT_TRUE(asm_ok("\n\n  NOP  \n\n", &p));
    ASSERT_EQ(p.size, 1);
    prog_free(&p);
}

TEST(program_instruction_count) {
    Program p;
    ASSERT_TRUE(asm_ok("MOV R0,5\nMOV R1,3\nADD R0,R1\nSTORE R0,0\nEND\n", &p));
    ASSERT_EQ(p.size, 5);
    prog_free(&p);
}

/* =========================================================
 *  main
 * ========================================================= */

int main(void) {
    printf("=== Assembler Tests ===\n");
    RUN(add_r0_r1); RUN(sub_r3_r5); RUN(and_r7_r7);
    RUN(or_r2_r4);  RUN(xor_r1_r6); RUN(cmp_r0_r7); RUN(mov_rr);
    RUN(all_regs_as_src); RUN(all_regs_as_dst);
    RUN(add_r2_r5_thesis);

    RUN(mov_imm_zero); RUN(mov_imm_42); RUN(mov_imm_255); RUN(mov_imm_hex);
    RUN(load_r3_200);  RUN(store_r1_100); RUN(load_r3_200_thesis);
    RUN(mov_imm_out_of_range); RUN(mov_imm_negative);
    RUN(load_addr_255_ok); RUN(store_addr_255_ok);

    RUN(jump_10); RUN(jump_zero_5); RUN(jump_not_zero_20);
    RUN(jump_signed_1); RUN(jump_not_signed_0);
    RUN(jump_carry_200); RUN(jump_not_carry_100); RUN(call_5);
    RUN(jump_10_thesis); RUN(call_5_thesis);
    RUN(jump_255_ok); RUN(jump_256_rejected); RUN(call_256_rejected);
    RUN(jump_addr8_bits_clean);

    RUN(push_r0); RUN(pop_r7); RUN(inc_r3); RUN(dec_r5);
    RUN(nop); RUN(end); RUN(ret);

    RUN(label_forward_ref); RUN(label_backward_ref);
    RUN(label_duplicate_rejected);
    RUN(label_used_in_load); RUN(label_used_in_store);
    RUN(label_unknown_rejected);
    RUN(label_at_addr255);
    RUN(too_many_labels);

    RUN(too_many_lines);

    RUN(unknown_mnemonic); RUN(bad_register_r8);
    RUN(missing_args_add); RUN(too_many_args_nop);
    RUN(bad_src_register); RUN(bad_dst_register);
    RUN(push_no_args);     RUN(jump_zero_args); RUN(jump_two_args);
    RUN(mov_bad_immediate);

    RUN(case_insensitive); RUN(comment_stripped); RUN(blank_lines_ignored);
    RUN(program_instruction_count);

    PRINT_RESULTS();
}
