/*
 * test_parser.c — Exhaustive unit tests for the VCL parser
 *
 * Target: 100% line coverage of parser.c (combined with test_codegen.c).
 * Covers every grammar construct, every error branch, and all capacity guards.
 *
 * Paths covered here that test_codegen.c does not hit:
 *   - Parenthesised expression: (expr)        [parse_primary L157-161]
 *   - Integer literal > 255                   [parse_primary L132-134]
 *   - Unexpected token in expression           [parse_primary L165-167]
 *   - Unexpected token in statement            [parse_stmt    L326-329]
 *   - Function with more than 3 parameters    [parse_func    L362]
 *   - Block with more than MAX_CHILDREN stmts [g_child_overflow L56+L400]
 *   - Call expression inside another expr     [parse_primary call path]
 *   - Call as statement (0 args)              [parse_stmt    L305-311]
 */

#ifndef VCPU_TESTING
#define VCPU_TESTING
#endif

#include "test_framework.h"
#include "compiler.h"
#include "lexer.c"
#include "parser.c"

static Token        toks[MAX_TOKENS];
static ErrorList el;

static ASTNode *do_parse(const char *src) {
    el.count = 0;
    ast_reset();
    int n = lex(src, toks, MAX_TOKENS, &el);
    if (n < 0 || el.count > 0) return NULL;
    return parse(toks, &el);
}

/* ---- program-level ---------------------------------------------------- */
TEST(empty_program) {
    ASTNode *r = do_parse("");
    ASSERT_TRUE(r != NULL);
    ASSERT_EQ(r->type, NODE_PROGRAM);
    ASSERT_EQ(r->nchildren, 0);
    ASSERT_EQ(el.count, 0);
}

TEST(global_var_no_init) {
    ASTNode *r = do_parse("var x;");
    ASSERT_EQ(el.count, 0);
    ASSERT_EQ(r->nchildren, 1);
    ASSERT_EQ(r->children[0]->type, NODE_VAR_DECL);
    ASSERT_TRUE(strcmp(r->children[0]->name, "x") == 0);
    ASSERT_EQ(r->children[0]->nchildren, 0);   /* no init expression */
}

TEST(global_var_with_init) {
    ASTNode *r = do_parse("var x = 42;");
    ASSERT_EQ(el.count, 0);
    ASTNode *v = r->children[0];
    ASSERT_EQ(v->nchildren, 1);
    ASSERT_EQ(v->children[0]->type, NODE_NUMBER);
    ASSERT_EQ(v->children[0]->ival, 42);
}

TEST(func_no_params) {
    ASTNode *r = do_parse("func foo() { return 0; }");
    ASSERT_EQ(el.count, 0);
    ASTNode *f = r->children[0];
    ASSERT_EQ(f->type, NODE_FUNC);
    ASSERT_TRUE(strcmp(f->name, "foo") == 0);
    ASSERT_EQ(f->nparams, 0);
    ASSERT_EQ(f->nchildren, 1);
}

TEST(func_one_param) {
    ASTNode *r = do_parse("func f(x) { return x; }");
    ASSERT_EQ(el.count, 0);
    ASSERT_EQ(r->children[0]->nparams, 1);
    ASSERT_TRUE(strcmp(r->children[0]->params[0], "x") == 0);
}

TEST(func_two_params) {
    ASTNode *r = do_parse("func add(a, b) { return a; }");
    ASSERT_EQ(el.count, 0);
    ASTNode *f = r->children[0];
    ASSERT_EQ(f->nparams, 2);
    ASSERT_TRUE(strcmp(f->params[0], "a") == 0);
    ASSERT_TRUE(strcmp(f->params[1], "b") == 0);
}

TEST(func_three_params) {
    ASTNode *r = do_parse("func sum3(a, b, c) { return a; }");
    ASSERT_EQ(el.count, 0);
    ASSERT_EQ(r->children[0]->nparams, 3);
}

TEST(multiple_funcs) {
    ASTNode *r = do_parse(
        "func f() { return 1; }"
        "func g() { return 2; }");
    ASSERT_EQ(el.count, 0);
    ASSERT_EQ(r->nchildren, 2);
}

/* ---- expressions ------------------------------------------------------ */
TEST(expr_number_literal) {
    ASTNode *r = do_parse("func f() { return 7; }");
    ASTNode *val = r->children[0]->children[0]->children[0]->children[0];
    ASSERT_EQ(el.count, 0);
    ASSERT_EQ(val->type, NODE_NUMBER);
    ASSERT_EQ(val->ival, 7);
}

TEST(expr_ident) {
    ASTNode *r = do_parse("func f(x) { return x; }");
    ASSERT_EQ(el.count, 0);
    ASTNode *val = r->children[0]->children[0]->children[0]->children[0];
    ASSERT_EQ(val->type, NODE_IDENT);
    ASSERT_TRUE(strcmp(val->name, "x") == 0);
}

TEST(expr_paren) {
    /* Parenthesised expression — exercises parse_primary L157-161 */
    ASTNode *r = do_parse("func f() { return (42); }");
    ASSERT_EQ(el.count, 0);
    ASTNode *val = r->children[0]->children[0]->children[0]->children[0];
    ASSERT_EQ(val->type, NODE_NUMBER);
    ASSERT_EQ(val->ival, 42);
}

TEST(expr_paren_binop) {
    ASTNode *r = do_parse("func f(a, b) { return (a + b); }");
    ASSERT_EQ(el.count, 0);
    ASTNode *val = r->children[0]->children[0]->children[0]->children[0];
    ASSERT_EQ(val->type, NODE_BINOP);
    ASSERT_EQ(val->ival, '+');
}

TEST(expr_binop_add) {
    ASTNode *r = do_parse("func f(a, b) { return a + b; }");
    ASSERT_EQ(el.count, 0);
    ASTNode *binop = r->children[0]->children[0]->children[0]->children[0];
    ASSERT_EQ(binop->type, NODE_BINOP);
    ASSERT_EQ(binop->ival, '+');
}

TEST(expr_binop_sub)   { ASTNode *r = do_parse("func f(a,b){return a-b;}"); ASSERT_EQ(el.count,0); ASSERT_EQ(r->children[0]->children[0]->children[0]->children[0]->ival,'-'); }
TEST(expr_binop_and)   { ASTNode *r = do_parse("func f(a,b){return a&b;}"); ASSERT_EQ(el.count,0); ASSERT_EQ(r->children[0]->children[0]->children[0]->children[0]->ival,'&'); }
TEST(expr_binop_or)    { ASTNode *r = do_parse("func f(a,b){return a|b;}"); ASSERT_EQ(el.count,0); ASSERT_EQ(r->children[0]->children[0]->children[0]->children[0]->ival,'|'); }
TEST(expr_binop_xor)   { ASTNode *r = do_parse("func f(a,b){return a^b;}"); ASSERT_EQ(el.count,0); ASSERT_EQ(r->children[0]->children[0]->children[0]->children[0]->ival,'^'); }

TEST(expr_cmp_eq)  { ASTNode *r = do_parse("func f(a,b){return a==b;}"); ASSERT_EQ(el.count,0); ASSERT_EQ(r->children[0]->children[0]->children[0]->children[0]->ival,BINOP_EQ);  }
TEST(expr_cmp_neq) { ASTNode *r = do_parse("func f(a,b){return a!=b;}"); ASSERT_EQ(el.count,0); ASSERT_EQ(r->children[0]->children[0]->children[0]->children[0]->ival,BINOP_NEQ); }
TEST(expr_cmp_lt)  { ASTNode *r = do_parse("func f(a,b){return a<b;}");  ASSERT_EQ(el.count,0); ASSERT_EQ(r->children[0]->children[0]->children[0]->children[0]->ival,'<');       }
TEST(expr_cmp_gt)  { ASTNode *r = do_parse("func f(a,b){return a>b;}");  ASSERT_EQ(el.count,0); ASSERT_EQ(r->children[0]->children[0]->children[0]->children[0]->ival,'>');       }
TEST(expr_cmp_leq) { ASTNode *r = do_parse("func f(a,b){return a<=b;}"); ASSERT_EQ(el.count,0); ASSERT_EQ(r->children[0]->children[0]->children[0]->children[0]->ival,BINOP_LEQ); }
TEST(expr_cmp_geq) { ASTNode *r = do_parse("func f(a,b){return a>=b;}"); ASSERT_EQ(el.count,0); ASSERT_EQ(r->children[0]->children[0]->children[0]->children[0]->ival,BINOP_GEQ); }

TEST(expr_chained_arith) {
    /* a + b + c -> left-associative: (a+b)+c */
    ASTNode *r = do_parse("func f(a,b,c){return a+b+c;}");
    ASSERT_EQ(el.count, 0);
    ASTNode *top = r->children[0]->children[0]->children[0]->children[0];
    ASSERT_EQ(top->type, NODE_BINOP);
    ASSERT_EQ(top->ival, '+');
    ASSERT_EQ(top->children[0]->type, NODE_BINOP); /* left is also + */
}

TEST(expr_call_in_expr) {
    ASTNode *r = do_parse("func f() { return foo(1, 2); }");
    ASSERT_EQ(el.count, 0);
    ASTNode *call = r->children[0]->children[0]->children[0]->children[0];
    ASSERT_EQ(call->type, NODE_CALL);
    ASSERT_TRUE(strcmp(call->name, "foo") == 0);
    ASSERT_EQ(call->nchildren, 2);
}

TEST(expr_call_no_args) {
    ASTNode *r = do_parse("func f() { return bar(); }");
    ASSERT_EQ(el.count, 0);
    ASTNode *call = r->children[0]->children[0]->children[0]->children[0];
    ASSERT_EQ(call->type, NODE_CALL);
    ASSERT_EQ(call->nchildren, 0);
}

/* ---- statements ------------------------------------------------------- */
TEST(stmt_var_decl_init) {
    ASTNode *r = do_parse("func f() { var x = 5; return x; }");
    ASSERT_EQ(el.count, 0);
    ASTNode *block = r->children[0]->children[0];
    ASSERT_EQ(block->children[0]->type, NODE_VAR_DECL);
    ASSERT_EQ(block->children[0]->nchildren, 1);
}

TEST(stmt_var_decl_no_init) {
    ASTNode *r = do_parse("func f() { var y; return 0; }");
    ASSERT_EQ(el.count, 0);
    ASSERT_EQ(r->children[0]->children[0]->children[0]->nchildren, 0);
}

TEST(stmt_assign) {
    ASTNode *r = do_parse("func f() { var x = 1; x = 2; }");
    ASSERT_EQ(el.count, 0);
    ASTNode *block = r->children[0]->children[0];
    ASSERT_EQ(block->children[1]->type, NODE_ASSIGN);
    ASSERT_TRUE(strcmp(block->children[1]->name, "x") == 0);
}

TEST(stmt_if_no_else) {
    ASTNode *r = do_parse("func f(x) { if (x < 5) { x = 0; } }");
    ASSERT_EQ(el.count, 0);
    ASTNode *if_node = r->children[0]->children[0]->children[0];
    ASSERT_EQ(if_node->type, NODE_IF);
    ASSERT_EQ(if_node->nchildren, 2);   /* cond + then only */
}

TEST(stmt_if_with_else) {
    ASTNode *r = do_parse("func f(x) { if (x < 5) { x = 0; } else { x = 1; } }");
    ASSERT_EQ(el.count, 0);
    ASSERT_EQ(r->children[0]->children[0]->children[0]->nchildren, 3);
}

TEST(stmt_while) {
    ASTNode *r = do_parse("func f() { while (1 < 2) { var x = 0; } }");
    ASSERT_EQ(el.count, 0);
    ASTNode *w = r->children[0]->children[0]->children[0];
    ASSERT_EQ(w->type, NODE_WHILE);
    ASSERT_EQ(w->nchildren, 2);
}

TEST(stmt_return_expr) {
    ASTNode *r = do_parse("func f() { return 99; }");
    ASSERT_EQ(el.count, 0);
    ASTNode *ret = r->children[0]->children[0]->children[0];
    ASSERT_EQ(ret->type, NODE_RETURN);
    ASSERT_EQ(ret->nchildren, 1);
    ASSERT_EQ(ret->children[0]->ival, 99);
}

TEST(stmt_call_as_stmt_no_args) {
    /* exercises the  if (check(p, TOK_LPAREN)) { ... }  branch in
       parse_stmt with zero arguments — parse_args with immediate ')' */
    ASTNode *r = do_parse("func f() { foo(); }");
    ASSERT_EQ(el.count, 0);
    ASTNode *call = r->children[0]->children[0]->children[0];
    ASSERT_EQ(call->type, NODE_CALL);
    ASSERT_TRUE(strcmp(call->name, "foo") == 0);
    ASSERT_EQ(call->nchildren, 0);
}

TEST(stmt_call_as_stmt_with_args) {
    ASTNode *r = do_parse("func f(a) { bar(a, 1); }");
    ASSERT_EQ(el.count, 0);
    ASTNode *call = r->children[0]->children[0]->children[0];
    ASSERT_EQ(call->type, NODE_CALL);
    ASSERT_EQ(call->nchildren, 2);
}

/* ---- error paths ------------------------------------------------------- */

TEST(err_num_out_of_range) {
    /* 256 > 255 — exercises the number-range check in parse_primary */
    do_parse("func f() { return 256; }");
    ASSERT_TRUE(el.count > 0);
    ASSERT_TRUE(strstr(el.msgs[0], "out of range") != NULL ||
                strstr(el.msgs[0], "256") != NULL);
}

TEST(err_unexpected_in_expr) {
    /* A keyword (not number/ident/paren) where a primary is expected
       exercises the  parse_err / return NULL  path in parse_primary */
    do_parse("func f() { return while; }");
    ASSERT_TRUE(el.count > 0);
}

TEST(err_unexpected_in_stmt) {
    /* A number literal where a statement is expected in a block — no
       branch (var/if/while/return/ident) handles TOK_NUMBER at statement
       position, so parse_stmt hits the default fall-through */
    do_parse("func f() { 42; }");
    ASSERT_TRUE(el.count > 0);
}

TEST(err_func_too_many_params) {
    /* 4 parameters exceeds MAX_PARAMS=3 — exercises parse_err("Function
       has more than 3 parameters") inside parse_func */
    do_parse("func f(a, b, c, d) { return 0; }");
    ASSERT_TRUE(el.count > 0);
    ASSERT_TRUE(strstr(el.msgs[0], "3 parameters") != NULL ||
                strstr(el.msgs[0], "more than") != NULL);
}

TEST(err_missing_semicolon) {
    do_parse("var x = 1");
    ASSERT_TRUE(el.count > 0);
}

TEST(err_bad_toplevel_token) {
    /* number literal at top level — exercises parse_err("Expected 'func'
       or 'var'...") in parse() and the ELSE branch of the while loop */
    do_parse("42 + 1;");
    ASSERT_TRUE(el.count > 0);
}

TEST(err_block_overflow) {
    /* Build a block with MAX_CHILDREN+1 var declarations so that the
       (MAX_CHILDREN+1)-th call to node_add_child triggers g_child_overflow.
       After parse() finishes the loop it emits the overflow error. */
    char src[8192];
    int pos = 0;
    pos += snprintf(src + pos, sizeof(src) - (size_t)pos, "func f() { ");
    for (int i = 0; i <= MAX_CHILDREN; i++)
        pos += snprintf(src + pos, sizeof(src) - (size_t)pos,
                        "var x%d=%d; ", i, i & 0xFF);
    snprintf(src + pos, sizeof(src) - (size_t)pos, "return 0; }");

    ASTNode *r = do_parse(src);
    ASSERT_TRUE(r != NULL);     /* parse() still returns a root node */
    ASSERT_TRUE(el.count > 0);  /* overflow error must be recorded   */
    ASSERT_TRUE(strstr(el.msgs[0], "MAX_CHILDREN") != NULL ||
                strstr(el.msgs[0], "too many") != NULL);
}

/* =========================================================
 *  main
 * ========================================================= */
int main(void) {
    RUN(empty_program);
    RUN(global_var_no_init); RUN(global_var_with_init);
    RUN(func_no_params); RUN(func_one_param); RUN(func_two_params);
    RUN(func_three_params); RUN(multiple_funcs);

    RUN(expr_number_literal); RUN(expr_ident);
    RUN(expr_paren); RUN(expr_paren_binop);
    RUN(expr_binop_add); RUN(expr_binop_sub);
    RUN(expr_binop_and); RUN(expr_binop_or); RUN(expr_binop_xor);
    RUN(expr_cmp_eq); RUN(expr_cmp_neq);
    RUN(expr_cmp_lt); RUN(expr_cmp_gt); RUN(expr_cmp_leq); RUN(expr_cmp_geq);
    RUN(expr_chained_arith);
    RUN(expr_call_in_expr); RUN(expr_call_no_args);

    RUN(stmt_var_decl_init); RUN(stmt_var_decl_no_init);
    RUN(stmt_assign);
    RUN(stmt_if_no_else); RUN(stmt_if_with_else);
    RUN(stmt_while);
    RUN(stmt_return_expr);
    RUN(stmt_call_as_stmt_no_args); RUN(stmt_call_as_stmt_with_args);

    RUN(err_num_out_of_range);
    RUN(err_unexpected_in_expr);
    RUN(err_unexpected_in_stmt);
    RUN(err_func_too_many_params);
    RUN(err_missing_semicolon);
    RUN(err_bad_toplevel_token);
    RUN(err_block_overflow);

    PRINT_RESULTS();
}
