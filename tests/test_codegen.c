/*
 * test_codegen.c — End-to-end and codegen unit tests for VCL compiler
 *
 * Target: 100% line coverage of codegen.c (combined with test_parser.c).
 * Every test compiles VCL source → assembles → runs through the emulator,
 * and checks the final value of R0.
 *
 * Paths covered here that test_parser.c does not hit:
 *   - out_printf overflow (tiny buffer)                 [codegen L48, L518-521]
 *   - gen_jump_if_false BINOP_EQ path (if/while ==)    [codegen L201-203]
 *   - gen_jump_if_false BINOP_NEQ path (if/while !=)   [codegen L204-206]
 *   - call with > 3 args (Too many arguments error)    [codegen gen_call_expr]
 *   - calling an undeclared function                   [codegen gen_call_expr]
 *   - function name used as a variable (x = foo)       [codegen gen_expr IDENT]
 *   - function name inside expression (x = foo + 1)    [codegen gen_expr IDENT]
 *   - undefined variable in ASSIGN                     [codegen NODE_ASSIGN]
 *   - NODE_CALL as standalone statement                [codegen L408-411]
 *   - global var with no initialiser in __start        [codegen L502-503]
 *   - parenthesised expressions in real programs       [parser  L157-161]
 */

#ifndef VCPU_TESTING
#define VCPU_TESTING
#endif

#include "test_framework.h"
#include "compiler.h"
#include "lexer.c"
#include "parser.c"
#include "semantic.c"
#include "codegen.c"

#include "assembler.h"
#include "assembler.c"
#include "emulator.c"

/* =========================================================
 *  Test helpers
 * ========================================================= */

#define ASM_BUF_SZ (64 * 1024)

/* Compile + assemble + run. Returns final R0 value, or -1 on error. */
static int run_vcl(const char *vcl_src) {
    char *asm_buf = (char *)malloc(ASM_BUF_SZ);
    if (!asm_buf) return -1;

    ErrorList cel;
    cel.count = 0;
    int ok = compile(vcl_src, asm_buf, ASM_BUF_SZ, &cel);
    if (!ok || cel.count > 0) {
        fprintf(stderr, "  compile: %s\n",
                cel.count ? cel.msgs[0] : "unknown error");
        free(asm_buf);
        return -1;
    }

    Program prog;
    ErrorList ael;
    ael.count = 0;
    assemble(asm_buf, &prog, &ael);
    free(asm_buf);
    if (ael.count > 0) {
        fprintf(stderr, "  assemble: %s\n", ael.msgs[0]);
        prog_free(&prog);
        return -1;
    }

    CPU cpu;
    cpu_reset(&cpu);
    run_program(&cpu, &prog, 200000, 0);
    prog_free(&prog);
    return (int)cpu.reg[0];
}

/* Compile source into a fixed-size ASM buffer.
   Returns 1 if compile() succeeds, 0 if it fails (error or buffer full). */
static int compile_to(const char *src, char *buf, int bufsz) {
    ErrorList el;
    el.count = 0;
    return compile(src, buf, bufsz, &el);
}

/* Returns 1 if compilation fails and the first error contains needle. */
static int compile_fails(const char *src, const char *needle) {
    char buf[ASM_BUF_SZ];
    ErrorList el;
    el.count = 0;
    compile(src, buf, ASM_BUF_SZ, &el);
    if (el.count == 0) return 0;
    if (needle && !strstr(el.msgs[0], needle)) return 0;
    return 1;
}

/* =========================================================
 *  Arithmetic expressions
 * ========================================================= */

TEST(return_constant)      { ASSERT_EQ(run_vcl("func main(){return 42;}"), 42); }
TEST(return_zero)          { ASSERT_EQ(run_vcl("func main(){return 0;}"),   0); }
TEST(return_max_byte)      { ASSERT_EQ(run_vcl("func main(){return 255;}"),255); }
TEST(return_addition)      { ASSERT_EQ(run_vcl("func main(){return 10+20;}"),    30); }
TEST(return_subtraction)   { ASSERT_EQ(run_vcl("func main(){return 20-7;}"),     13); }
TEST(return_bitwise_and)   { ASSERT_EQ(run_vcl("func main(){return 0xFF&0x0F;}"),15); }
TEST(return_bitwise_or)    { ASSERT_EQ(run_vcl("func main(){return 0xF0|0x0F;}"),255);}
TEST(return_bitwise_xor)   { ASSERT_EQ(run_vcl("func main(){return 0xFF^0x0F;}"),240);}
TEST(nested_arithmetic)    { ASSERT_EQ(run_vcl("func main(){return 3+4-1-1;}"),  5);  }

TEST(paren_expr_simple) {
    /* parenthesised expression — exercises parse_primary L157-161 in an
       actual compile+run context */
    ASSERT_EQ(run_vcl("func main() { return (10 + 5); }"), 15);
}

TEST(paren_expr_nested) {
    ASSERT_EQ(run_vcl("func main() { return (3 + (4 + 5)); }"), 12);
}

TEST(paren_in_condition) {
    ASSERT_EQ(run_vcl("func main() { if ((1 + 1) == 2) { return 1; } return 0; }"), 1);
}

/* =========================================================
 *  Variables
 * ========================================================= */

TEST(local_var_init)         { ASSERT_EQ(run_vcl("func main(){var x=7;return x;}"),  7); }
TEST(local_var_default_zero) { ASSERT_EQ(run_vcl("func main(){var x;return x;}"),    0); }
TEST(local_var_reassign)     { ASSERT_EQ(run_vcl("func main(){var x=1;x=99;return x;}"), 99); }

TEST(global_var_with_init) {
    ASSERT_EQ(run_vcl(
        "var g = 55;\n"
        "func main() { return g; }"), 55);
}

TEST(global_var_write) {
    ASSERT_EQ(run_vcl(
        "var g = 0;\n"
        "func main() { g = 33; return g; }"), 33);
}

TEST(global_var_no_init) {
    /* var g; without initialiser — exercises codegen __start path that
       emits  MOV R0, 0  /  STORE R0, addr  (codegen L502-503) */
    ASSERT_EQ(run_vcl(
        "var counter;\n"
        "func main() { counter = 7; return counter; }"), 7);
}

TEST(global_and_local) {
    ASSERT_EQ(run_vcl(
        "var g = 10;\n"
        "func main() { var x = g; return x + 5; }"), 15);
}

/* =========================================================
 *  Comparisons as values (gen_jump_if_true)
 * ========================================================= */

TEST(cmp_eq_true)    { ASSERT_EQ(run_vcl("func main(){return 5==5;}"), 1); }
TEST(cmp_eq_false)   { ASSERT_EQ(run_vcl("func main(){return 5==6;}"), 0); }
TEST(cmp_neq_true)   { ASSERT_EQ(run_vcl("func main(){return 5!=6;}"), 1); }
TEST(cmp_neq_false)  { ASSERT_EQ(run_vcl("func main(){return 5!=5;}"), 0); }
TEST(cmp_lt_true)    { ASSERT_EQ(run_vcl("func main(){return 3<5;}"),  1); }
TEST(cmp_lt_false)   { ASSERT_EQ(run_vcl("func main(){return 5<3;}"),  0); }
TEST(cmp_gt_true)    { ASSERT_EQ(run_vcl("func main(){return 5>3;}"),  1); }
TEST(cmp_gt_false)   { ASSERT_EQ(run_vcl("func main(){return 3>5;}"),  0); }
TEST(cmp_leq_equal)  { ASSERT_EQ(run_vcl("func main(){return 5<=5;}"), 1); }
TEST(cmp_leq_less)   { ASSERT_EQ(run_vcl("func main(){return 4<=5;}"), 1); }
TEST(cmp_leq_false)  { ASSERT_EQ(run_vcl("func main(){return 6<=5;}"), 0); }
TEST(cmp_geq_equal)  { ASSERT_EQ(run_vcl("func main(){return 5>=5;}"), 1); }
TEST(cmp_geq_great)  { ASSERT_EQ(run_vcl("func main(){return 6>=5;}"), 1); }
TEST(cmp_geq_false)  { ASSERT_EQ(run_vcl("func main(){return 4>=5;}"), 0); }

/* =========================================================
 *  if / else — all six comparison ops as CONDITIONS
 *  (gen_jump_if_false is always emitted; this section ensures EQ and
 *  NEQ are exercised there, which test_parser.c does not reach)
 * ========================================================= */

TEST(if_cond_lt_taken)   { ASSERT_EQ(run_vcl("func main(){if(1<2){return 1;}return 0;}"), 1); }
TEST(if_cond_lt_skipped) { ASSERT_EQ(run_vcl("func main(){if(2<1){return 1;}return 0;}"), 0); }
TEST(if_cond_gt_taken)   { ASSERT_EQ(run_vcl("func main(){if(5>3){return 1;}return 0;}"), 1); }
TEST(if_cond_gt_skipped) { ASSERT_EQ(run_vcl("func main(){if(3>5){return 1;}return 0;}"), 0); }
TEST(if_cond_leq_taken)  { ASSERT_EQ(run_vcl("func main(){if(5<=5){return 1;}return 0;}"),1); }
TEST(if_cond_geq_taken)  { ASSERT_EQ(run_vcl("func main(){if(5>=5){return 1;}return 0;}"),1); }

TEST(if_cond_eq_taken) {
    /* exercises gen_jump_if_false BINOP_EQ branch (L201-203):
       condition is TRUE  → JUMP_NOT_ZERO is NOT taken → then executes */
    ASSERT_EQ(run_vcl("func main() { if (5 == 5) { return 10; } return 0; }"), 10);
}

TEST(if_cond_eq_skipped) {
    /* condition is FALSE → JUMP_NOT_ZERO IS taken → else/end */
    ASSERT_EQ(run_vcl("func main() { if (5 == 6) { return 10; } return 0; }"), 0);
}

TEST(if_cond_neq_taken) {
    /* exercises gen_jump_if_false BINOP_NEQ branch (L204-206):
       condition is TRUE  → JUMP_ZERO is NOT taken → then executes */
    ASSERT_EQ(run_vcl("func main() { if (5 != 6) { return 10; } return 0; }"), 10);
}

TEST(if_cond_neq_skipped) {
    /* condition is FALSE → JUMP_ZERO IS taken → end */
    ASSERT_EQ(run_vcl("func main() { if (5 != 5) { return 10; } return 0; }"), 0);
}

TEST(if_else_then_path) {
    ASSERT_EQ(run_vcl(
        "func main() { if (5>3) { return 1; } else { return 0; } }"), 1);
}

TEST(if_else_else_path) {
    ASSERT_EQ(run_vcl(
        "func main() { if (3>5) { return 1; } else { return 0; } }"), 0);
}

/* =========================================================
 *  while — all comparison ops as loop conditions
 * ========================================================= */

TEST(while_lt_count_to_5) {
    ASSERT_EQ(run_vcl(
        "func main() { var i=0; while(i<5){i=i+1;} return i; }"), 5);
}

TEST(while_never_entered) {
    ASSERT_EQ(run_vcl(
        "func main() { var i=10; while(i<5){i=i+1;} return i; }"), 10);
}

TEST(while_leq_sum_1_to_10) {
    /* exercises gen_jump_if_false BINOP_LEQ; result = 1+2+…+10 = 55 */
    ASSERT_EQ(run_vcl(
        "func main() {"
        "  var sum=0; var i=1;"
        "  while(i<=10){ sum=sum+i; i=i+1; }"
        "  return sum;"
        "}"), 55);
}

TEST(while_cond_eq) {
    /* exercises gen_jump_if_false BINOP_EQ in a while loop header */
    ASSERT_EQ(run_vcl(
        "func main() {"
        "  var a=0;"
        "  while(a==0){ a=1; }"
        "  return a;"
        "}"), 1);
}

TEST(while_cond_neq) {
    /* exercises gen_jump_if_false BINOP_NEQ in a while loop header */
    ASSERT_EQ(run_vcl(
        "func main() {"
        "  var a=0;"
        "  while(a!=5){ a=a+1; }"
        "  return a;"
        "}"), 5);
}

TEST(while_cond_gt) {
    ASSERT_EQ(run_vcl(
        "func main() { var a=10; while(a>0){ a=a-1; } return a; }"), 0);
}

TEST(while_cond_geq) {
    ASSERT_EQ(run_vcl(
        "func main() { var a=5; while(a>=1){ a=a-1; } return a; }"), 0);
}

/* =========================================================
 *  Function calls
 * ========================================================= */

TEST(call_no_args) {
    ASSERT_EQ(run_vcl(
        "func forty_two() { return 42; }"
        "func main() { return forty_two(); }"), 42);
}

TEST(call_one_arg) {
    ASSERT_EQ(run_vcl(
        "func double_val(x) { return x+x; }"
        "func main() { return double_val(7); }"), 14);
}

TEST(call_two_args) {
    ASSERT_EQ(run_vcl(
        "func add(a, b) { return a+b; }"
        "func main() { return add(10, 20); }"), 30);
}

TEST(call_three_args) {
    ASSERT_EQ(run_vcl(
        "func sum3(a, b, c) { return a+b+c; }"
        "func main() { return sum3(1, 2, 3); }"), 6);
}

TEST(call_chained) {
    ASSERT_EQ(run_vcl(
        "func add(a, b) { return a+b; }"
        "func main() { var x=add(10,20); return add(x,5); }"), 35);
}

TEST(standalone_call_stmt) {
    /* call used as a statement (return value discarded) — exercises
       gen_stmt NODE_CALL path (codegen L408-411) */
    ASSERT_EQ(run_vcl(
        "func noop(x) { return x; }"
        "func main() {"
        "  noop(42);"     /* result discarded */
        "  return 7;"
        "}"), 7);
}

TEST(call_stmt_side_effect) {
    /* standalone call that modifies a global */
    ASSERT_EQ(run_vcl(
        "var g = 0;\n"
        "func set_g(v) { g = v; return 0; }"
        "func main() {"
        "  set_g(99);"
        "  return g;"
        "}"), 99);
}

/* =========================================================
 *  Larger algorithmic programs
 * ========================================================= */

TEST(max_function) {
    ASSERT_EQ(run_vcl(
        "func max(a,b) { if(a>b){return a;} return b; }"
        "func main() { return max(42,17); }"), 42);
}

TEST(max_reversed_args) {
    ASSERT_EQ(run_vcl(
        "func max(a,b) { if(a>b){return a;} return b; }"
        "func main() { return max(3,200); }"), 200);
}

TEST(fibonacci_iterative) {
    /* fib(10) = 55 */
    ASSERT_EQ(run_vcl(
        "func fib(n) {"
        "  var a=0; var b=1; var i=0;"
        "  while(i<n) {"
        "    var tmp=b; b=a+b; a=tmp;"
        "    i=i+1;"
        "  }"
        "  return a;"
        "}"
        "func main() { return fib(10); }"), 55);
}

TEST(gcd_function) {
    /* gcd(48, 36) = 12  — Euclidean subtraction */
    ASSERT_EQ(run_vcl(
        "func gcd(a, b) {"
        "  while(a != b) {"
        "    if(a>b){a=a-b;} else {b=b-a;}"
        "  }"
        "  return a;"
        "}"
        "func main() { return gcd(48, 36); }"), 12);
}

TEST(clamp_above_hi) {
    ASSERT_EQ(run_vcl(
        "func clamp(val, lo, hi) {"
        "  if(val<lo){return lo;}"
        "  if(val>hi){return hi;}"
        "  return val;"
        "}"
        "func main() { return clamp(200, 10, 100); }"), 100);
}

TEST(clamp_below_lo) {
    ASSERT_EQ(run_vcl(
        "func clamp(val, lo, hi) {"
        "  if(val<lo){return lo;}"
        "  if(val>hi){return hi;}"
        "  return val;"
        "}"
        "func main() { return clamp(3, 10, 100); }"), 10);
}

TEST(clamp_in_range) {
    ASSERT_EQ(run_vcl(
        "func clamp(val, lo, hi) {"
        "  if(val<lo){return lo;}"
        "  if(val>hi){return hi;}"
        "  return val;"
        "}"
        "func main() { return clamp(50, 10, 100); }"), 50);
}

TEST(power_2_to_7) {
    /* 2^7 = 128 using a multiply helper */
    ASSERT_EQ(run_vcl(
        "func multiply(a, b) {"
        "  var result=0; var i=0;"
        "  while(i<b){ result=result+a; i=i+1; }"
        "  return result;"
        "}"
        "func power(base, exp) {"
        "  var result=1; var i=0;"
        "  while(i<exp){ result=multiply(result,base); i=i+1; }"
        "  return result;"
        "}"
        "func main() { return power(2, 7); }"), 128);
}

TEST(min3_function) {
    ASSERT_EQ(run_vcl(
        "func min2(a, b) { if(a<b){return a;} return b; }"
        "func min3(a, b, c) { return min2(min2(a,b),c); }"
        "func main() { return min3(42, 17, 99); }"), 17);
}

/* =========================================================
 *  Error detection
 * ========================================================= */

TEST(err_undefined_var_in_expr) {
    /* undefined var used in expression (gen_expr IDENT path) */
    ASSERT_TRUE(compile_fails(
        "func main() { return x; }", "Undefined variable 'x'"));
}

TEST(err_undefined_var_in_assign) {
    /* undefined var used in ASSIGN — exercises codegen L364-366,
       which is a DIFFERENT path from the IDENT expression path */
    ASSERT_TRUE(compile_fails(
        "func main() { x = 5; return 0; }", "Undefined variable 'x'"));
}

TEST(err_too_many_args) {
    /* 4 arguments to a declared call — parser allows it, codegen rejects */
    ASSERT_TRUE(compile_fails(
        "func foo(a,b,c){return a;} func main() { return foo(1,2,3,4); }",
        "Too many arguments"));
}

TEST(err_undeclared_function) {
    /* calling a function that was never declared */
    ASSERT_TRUE(compile_fails(
        "func main() { return undeclared(); }", "Undefined function 'undeclared'"));
}

TEST(err_func_as_variable) {
    /* using a function name as the RHS of an assignment */
    ASSERT_TRUE(compile_fails(
        "func helper() { return 1; }"
        "func main() { var x; x = helper; return x; }",
        "is a function, not a variable"));
}

TEST(err_func_in_expr) {
    /* using a function name inside an arithmetic expression */
    ASSERT_TRUE(compile_fails(
        "func helper() { return 1; }"
        "func main() { var x; x = helper + 1; return x; }",
        "is a function, not a variable"));
}

TEST(err_lex_propagates) {
    /* lexer error stops compilation */
    ASSERT_TRUE(compile_fails("func main() { return @; }", NULL));
}

TEST(err_parse_propagates) {
    /* parser error stops compilation */
    ASSERT_TRUE(compile_fails("func main() { return 1 }", NULL));
}

TEST(err_output_buffer_overflow) {
    /* a buffer that is far too small to hold any assembly output —
       exercises out_printf overflow (codegen L48) and the overflow
       check at the end of codegen() (L518-521) */
    char tiny[20];
    int ok = compile_to("func main() { return 1; }", tiny, (int)sizeof(tiny));
    ASSERT_TRUE(!ok);
}

/* =========================================================
 *  main
 * ========================================================= */
int main(void) {
    RUN(return_constant); RUN(return_zero); RUN(return_max_byte);
    RUN(return_addition); RUN(return_subtraction);
    RUN(return_bitwise_and); RUN(return_bitwise_or); RUN(return_bitwise_xor);
    RUN(nested_arithmetic);
    RUN(paren_expr_simple); RUN(paren_expr_nested); RUN(paren_in_condition);

    RUN(local_var_init); RUN(local_var_default_zero); RUN(local_var_reassign);
    RUN(global_var_with_init); RUN(global_var_write); RUN(global_var_no_init);
    RUN(global_and_local);

    RUN(cmp_eq_true); RUN(cmp_eq_false);
    RUN(cmp_neq_true); RUN(cmp_neq_false);
    RUN(cmp_lt_true); RUN(cmp_lt_false);
    RUN(cmp_gt_true); RUN(cmp_gt_false);
    RUN(cmp_leq_equal); RUN(cmp_leq_less); RUN(cmp_leq_false);
    RUN(cmp_geq_equal); RUN(cmp_geq_great); RUN(cmp_geq_false);

    RUN(if_cond_lt_taken); RUN(if_cond_lt_skipped);
    RUN(if_cond_gt_taken); RUN(if_cond_gt_skipped);
    RUN(if_cond_leq_taken); RUN(if_cond_geq_taken);
    RUN(if_cond_eq_taken); RUN(if_cond_eq_skipped);
    RUN(if_cond_neq_taken); RUN(if_cond_neq_skipped);
    RUN(if_else_then_path); RUN(if_else_else_path);

    RUN(while_lt_count_to_5); RUN(while_never_entered);
    RUN(while_leq_sum_1_to_10);
    RUN(while_cond_eq); RUN(while_cond_neq);
    RUN(while_cond_gt); RUN(while_cond_geq);

    RUN(call_no_args); RUN(call_one_arg);
    RUN(call_two_args); RUN(call_three_args); RUN(call_chained);
    RUN(standalone_call_stmt); RUN(call_stmt_side_effect);

    RUN(max_function); RUN(max_reversed_args);
    RUN(fibonacci_iterative);
    RUN(gcd_function);
    RUN(clamp_above_hi); RUN(clamp_below_lo); RUN(clamp_in_range);
    RUN(power_2_to_7);
    RUN(min3_function);

    RUN(err_undefined_var_in_expr);
    RUN(err_undefined_var_in_assign);
    RUN(err_too_many_args);
    RUN(err_undeclared_function);
    RUN(err_func_as_variable);
    RUN(err_func_in_expr);
    RUN(err_lex_propagates);
    RUN(err_parse_propagates);
    RUN(err_output_buffer_overflow);

    PRINT_RESULTS();
}
