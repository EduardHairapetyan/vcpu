/*
 * test_semantic.c — Unit tests for the VCL semantic analysis pass (C99)
 *
 * Tests exercise semantic() directly: after lex+parse, before codegen.
 * Every case that should fail asserts el.count > 0 and checks the
 * first error message contains the expected substring.
 */

#ifndef VCPU_TESTING
#define VCPU_TESTING
#endif

#include "test_framework.h"
#include "compiler.h"
#include "lexer.c"
#include "parser.c"
#include "semantic.c"

/* =========================================================
 *  Helpers
 * ========================================================= */

/* Run lex+parse+semantic.  Returns the semantic() return value.
   el->count reflects only errors from semantic (parse errors are cleared). */
static int run_semantic(const char *src, ErrorList *el) {
    Token tokens[MAX_TOKENS];
    ast_reset();
    ErrorList lex_el;
    lex_el.count = 0;
    int ntok = lex(src, tokens, MAX_TOKENS, &lex_el);
    if (ntok < 0 || lex_el.count > 0) return -1; /* bad test input */

    ASTNode *root = parse(tokens, &lex_el);
    if (!root || lex_el.count > 0) return -1;     /* bad test input */

    return semantic(root, el);
}

/* Assert that src produces at least one semantic error containing needle. */
static void expect_error(const char *src, const char *needle) {
    ErrorList el;
    el.count = 0;
    int ok = run_semantic(src, &el);
    ASSERT_FALSE(ok);
    ASSERT_TRUE(el.count > 0);
    if (needle)
        ASSERT_STR_CONTAINS(el.msgs[0], needle);
}

/* Assert that src passes semantic analysis with no errors. */
static void expect_ok(const char *src) {
    ErrorList el;
    el.count = 0;
    int ok = run_semantic(src, &el);
    ASSERT_TRUE(ok);
    ASSERT_EQ(el.count, 0);
}

/* =========================================================
 *  Undeclared variable
 * ========================================================= */

TEST(undeclared_var_in_return) {
    expect_error(
        "func main() { return x; }",
        "Undefined variable 'x'");
}

TEST(undeclared_var_in_assign) {
    expect_error(
        "func main() { x = 1; return 0; }",
        "Undefined variable 'x'");
}

TEST(undeclared_var_in_expr) {
    expect_error(
        "func main() { return x + 1; }",
        "Undefined variable 'x'");
}

/* =========================================================
 *  Undeclared function call
 * ========================================================= */

TEST(undeclared_function) {
    expect_error(
        "func main() { return foo(); }",
        "Undefined function 'foo'");
}

TEST(undeclared_function_as_stmt) {
    expect_error(
        "func main() { bar(); return 0; }",
        "Undefined function 'bar'");
}

/* =========================================================
 *  Function name used as a variable
 * ========================================================= */

TEST(func_as_variable_in_return) {
    expect_error(
        "func helper() { return 1; }"
        "func main() { return helper; }",
        "is a function, not a variable");
}

TEST(func_as_variable_in_assign_rhs) {
    expect_error(
        "func helper() { return 1; }"
        "func main() { var x; x = helper; return x; }",
        "is a function, not a variable");
}

TEST(func_as_variable_in_expr) {
    expect_error(
        "func helper() { return 1; }"
        "func main() { var x; x = helper + 1; return x; }",
        "is a function, not a variable");
}

/* =========================================================
 *  Variable name called as a function
 * ========================================================= */

TEST(variable_as_function) {
    expect_error(
        "func main() { var x; x(); return 0; }",
        "'x' is not a function");
}

TEST(variable_as_function_in_expr) {
    expect_error(
        "func main() { var x; return x(); }",
        "'x' is not a function");
}

/* =========================================================
 *  Wrong argument count
 * ========================================================= */

TEST(too_few_args) {
    expect_error(
        "func add(a, b) { return a + b; }"
        "func main() { return add(1); }",
        "Too few arguments");
}

TEST(too_few_args_zero) {
    expect_error(
        "func one(a) { return a; }"
        "func main() { return one(); }",
        "Too few arguments");
}

TEST(too_many_args) {
    expect_error(
        "func add(a, b) { return a + b; }"
        "func main() { return add(1, 2, 3); }",
        "Too many arguments");
}

TEST(too_many_args_four) {
    /* parser allows 4 args in a call; semantic rejects against nparams=3 */
    expect_error(
        "func foo(a, b, c) { return a; }"
        "func main() { return foo(1, 2, 3, 4); }",
        "Too many arguments");
}

/* =========================================================
 *  Duplicate declarations
 * ========================================================= */

TEST(duplicate_local_var) {
    expect_error(
        "func main() { var x; var x; return 0; }",
        "Duplicate declaration of 'x'");
}

TEST(duplicate_global_var) {
    expect_error(
        "var g;"
        "var g;"
        "func main() { return 0; }",
        "Duplicate declaration of 'g'");
}

TEST(duplicate_function) {
    expect_error(
        "func foo() { return 1; }"
        "func foo() { return 2; }"
        "func main() { return 0; }",
        "Duplicate function 'foo'");
}

TEST(duplicate_param_as_local) {
    /* re-declaring a parameter as a local variable in the same scope */
    expect_error(
        "func foo(a) { var a; return a; }"
        "func main() { return foo(1); }",
        "Duplicate declaration of 'a'");
}

/* =========================================================
 *  Valid programs (no errors)
 * ========================================================= */

TEST(valid_minimal) {
    expect_ok("func main() { return 0; }");
}

TEST(valid_forward_call) {
    /* main calls bar which is declared after main — forward call is OK */
    expect_ok(
        "func main() { return bar(); }"
        "func bar() { return 42; }");
}

TEST(valid_mutual_call) {
    expect_ok(
        "func even(n) { if (n == 0) { return 1; } return odd(n); }"
        "func odd(n)  { if (n == 0) { return 0; } return even(n); }"
        "func main()  { return even(4); }");
}

TEST(valid_global_and_local) {
    expect_ok(
        "var g = 10;"
        "func add(a, b) { return a + b; }"
        "func main() { var x; x = add(g, 5); return x; }");
}

TEST(valid_local_shadows_global) {
    /* local var with the same name as a global — shadowing is allowed */
    expect_ok(
        "var x = 1;"
        "func main() { var x; x = 2; return x; }");
}

/* =========================================================
 *  main
 * ========================================================= */

int main(void) {
    RUN(undeclared_var_in_return);
    RUN(undeclared_var_in_assign);
    RUN(undeclared_var_in_expr);

    RUN(undeclared_function);
    RUN(undeclared_function_as_stmt);

    RUN(func_as_variable_in_return);
    RUN(func_as_variable_in_assign_rhs);
    RUN(func_as_variable_in_expr);

    RUN(variable_as_function);
    RUN(variable_as_function_in_expr);

    RUN(too_few_args);
    RUN(too_few_args_zero);
    RUN(too_many_args);
    RUN(too_many_args_four);

    RUN(duplicate_local_var);
    RUN(duplicate_global_var);
    RUN(duplicate_function);
    RUN(duplicate_param_as_local);

    RUN(valid_minimal);
    RUN(valid_forward_call);
    RUN(valid_mutual_call);
    RUN(valid_global_and_local);
    RUN(valid_local_shadows_global);

    PRINT_RESULTS();
}
