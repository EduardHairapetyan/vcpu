/*
 * test_lexer.c — Exhaustive unit tests for the VCL lexer
 *
 * Target: 100% line and branch coverage of lexer.c including:
 *   - All keyword types
 *   - Identifier edge cases (underscore-start, long name truncation)
 *   - Decimal and hex integer literals (lowercase and uppercase 0x/0X)
 *   - All two-character operators
 *   - All single-character tokens
 *   - Comment stripping: mid-line, EOF-terminated (no trailing newline)
 *   - Whitespace and newline handling (line-counter tracking)
 *   - Unknown character -> error recorded (default: branch + el_add body)
 *   - el_add overflow guard (el already full)
 *   - Token-buffer overflow (too many tokens)
 */

#ifndef VCPU_TESTING
#define VCPU_TESTING
#endif

#include "test_framework.h"
#include "compiler.h"
#include "lexer.c"

static Token toks[MAX_TOKENS];
static ErrorList el;

static int do_lex(const char *src) {
    el.count = 0;
    return lex(src, toks, MAX_TOKENS, &el);
}

/* ---- keywords ---------------------------------------------------------- */
TEST(kw_func)   { do_lex("func");   ASSERT_EQ(toks[0].type, TOK_FUNC);   }
TEST(kw_var)    { do_lex("var");    ASSERT_EQ(toks[0].type, TOK_VAR);    }
TEST(kw_if)     { do_lex("if");     ASSERT_EQ(toks[0].type, TOK_IF);     }
TEST(kw_else)   { do_lex("else");   ASSERT_EQ(toks[0].type, TOK_ELSE);   }
TEST(kw_while)  { do_lex("while");  ASSERT_EQ(toks[0].type, TOK_WHILE);  }
TEST(kw_return) { do_lex("return"); ASSERT_EQ(toks[0].type, TOK_RETURN); }

/* ---- identifiers ------------------------------------------------------- */
TEST(ident_simple) {
    do_lex("hello");
    ASSERT_EQ(toks[0].type, TOK_IDENT);
    ASSERT_TRUE(strcmp(toks[0].text, "hello") == 0);
}

TEST(ident_underscore_start) {
    do_lex("_x");
    ASSERT_EQ(toks[0].type, TOK_IDENT);
    ASSERT_TRUE(strcmp(toks[0].text, "_x") == 0);
}

TEST(ident_with_digits) {
    do_lex("x1");
    ASSERT_EQ(toks[0].type, TOK_IDENT);
}

TEST(kw_not_ident_prefix) {
    /* "function" starts with keyword "func" but is a distinct identifier */
    do_lex("function");
    ASSERT_EQ(toks[0].type, TOK_IDENT);
    ASSERT_TRUE(strcmp(toks[0].text, "function") == 0);
}

TEST(ident_truncated_at_max) {
    /* name > MAX_NAME-1 chars must be silently truncated */
    char long_id[MAX_NAME + 8];
    memset(long_id, 'a', MAX_NAME + 4);
    long_id[MAX_NAME + 4] = '\0';
    do_lex(long_id);
    ASSERT_EQ(toks[0].type, TOK_IDENT);
    ASSERT_EQ((int)strlen(toks[0].text), MAX_NAME - 1);
}

/* ---- integer literals -------------------------------------------------- */
TEST(num_zero)    { do_lex("0");    ASSERT_EQ(toks[0].type, TOK_NUMBER); }
TEST(num_decimal) { do_lex("42");   ASSERT_EQ(toks[0].type, TOK_NUMBER); ASSERT_TRUE(strcmp(toks[0].text,"42")==0); }
TEST(num_max_255) { do_lex("255");  ASSERT_EQ(toks[0].type, TOK_NUMBER); }
TEST(num_hex_lower) { do_lex("0x1a"); ASSERT_EQ(toks[0].type, TOK_NUMBER); ASSERT_TRUE(strcmp(toks[0].text,"0x1a")==0); }
TEST(num_hex_upper) { do_lex("0xFF"); ASSERT_EQ(toks[0].type, TOK_NUMBER); ASSERT_TRUE(strcmp(toks[0].text,"0xFF")==0); }
TEST(num_hex_X)     {
    /* 0X (uppercase X) prefix — exercises the `p[1]=='X'` branch */
    do_lex("0X10");
    ASSERT_EQ(toks[0].type, TOK_NUMBER);
    ASSERT_TRUE(strcmp(toks[0].text,"0X10")==0);
}

/* ---- two-character operators ------------------------------------------- */
TEST(op_eq)  { do_lex("=="); ASSERT_EQ(toks[0].type, TOK_EQ);  ASSERT_EQ(el.count,0); }
TEST(op_neq) { do_lex("!="); ASSERT_EQ(toks[0].type, TOK_NEQ); ASSERT_EQ(el.count,0); }
TEST(op_leq) { do_lex("<="); ASSERT_EQ(toks[0].type, TOK_LEQ); ASSERT_EQ(el.count,0); }
TEST(op_geq) { do_lex(">="); ASSERT_EQ(toks[0].type, TOK_GEQ); ASSERT_EQ(el.count,0); }

TEST(op_assign_vs_eq) {
    do_lex("= ==");
    ASSERT_EQ(toks[0].type, TOK_ASSIGN);
    ASSERT_EQ(toks[1].type, TOK_EQ);
}
TEST(op_lt_vs_leq) {
    do_lex("< <=");
    ASSERT_EQ(toks[0].type, TOK_LT);
    ASSERT_EQ(toks[1].type, TOK_LEQ);
}
TEST(op_gt_vs_geq) {
    do_lex("> >=");
    ASSERT_EQ(toks[0].type, TOK_GT);
    ASSERT_EQ(toks[1].type, TOK_GEQ);
}

/* ---- single-character arithmetic / bitwise ----------------------------- */
TEST(ops_arith_all) {
    do_lex("+ - & | ^");
    ASSERT_EQ(toks[0].type, TOK_PLUS);
    ASSERT_EQ(toks[1].type, TOK_MINUS);
    ASSERT_EQ(toks[2].type, TOK_AMP);
    ASSERT_EQ(toks[3].type, TOK_PIPE);
    ASSERT_EQ(toks[4].type, TOK_CARET);
    ASSERT_EQ(el.count, 0);
}

/* ---- punctuation ------------------------------------------------------- */
TEST(punct_all) {
    do_lex("( ) { } , ;");
    ASSERT_EQ(toks[0].type, TOK_LPAREN);
    ASSERT_EQ(toks[1].type, TOK_RPAREN);
    ASSERT_EQ(toks[2].type, TOK_LBRACE);
    ASSERT_EQ(toks[3].type, TOK_RBRACE);
    ASSERT_EQ(toks[4].type, TOK_COMMA);
    ASSERT_EQ(toks[5].type, TOK_SEMICOLON);
    ASSERT_EQ(el.count, 0);
}

/* ---- comments ---------------------------------------------------------- */
TEST(comment_mid_line) {
    int n = do_lex("42 // ignored\n99");
    ASSERT_EQ(n, 2);
    ASSERT_EQ(toks[0].type, TOK_NUMBER);
    ASSERT_TRUE(strcmp(toks[0].text,"42")==0);
    ASSERT_EQ(toks[1].type, TOK_NUMBER);
    ASSERT_TRUE(strcmp(toks[1].text,"99")==0);
}

TEST(comment_to_eof_no_newline) {
    /* exercises the  while (*p && *p != '\n') p++;  loop terminating at '\0'
       (the !*p condition), which was previously uncovered */
    int n = do_lex("1 // no newline at end");
    ASSERT_EQ(n, 1);
    ASSERT_EQ(toks[0].type, TOK_NUMBER);
}

TEST(comment_only) {
    int n = do_lex("// entire file is a comment");
    ASSERT_EQ(n, 0);
    ASSERT_EQ(toks[0].type, TOK_EOF);
}

/* ---- whitespace and line tracking -------------------------------------- */
TEST(whitespace_skipped) {
    int n = do_lex("  \t  42  \n  ");
    ASSERT_EQ(n, 1);
    ASSERT_EQ(toks[0].type, TOK_NUMBER);
}

TEST(eof_empty_source) {
    int n = do_lex("");
    ASSERT_EQ(n, 0);
    ASSERT_EQ(toks[0].type, TOK_EOF);
}

TEST(line_numbers_track) {
    do_lex("a\nb\nc");
    ASSERT_EQ(toks[0].line, 1);
    ASSERT_EQ(toks[1].line, 2);
    ASSERT_EQ(toks[2].line, 3);
}

/* ---- error paths ------------------------------------------------------- */

TEST(unknown_char_at) {
    /* '@' is not a valid token — exercises the default: branch of the
       switch and the full body of el_add() */
    el.count = 0;
    lex("@", toks, MAX_TOKENS, &el);
    ASSERT_TRUE(el.count > 0);
    ASSERT_TRUE(strstr(el.msgs[0], "'@'") != NULL);
}

TEST(unknown_char_hash) {
    el.count = 0;
    lex("#", toks, MAX_TOKENS, &el);
    ASSERT_TRUE(el.count > 0);
    ASSERT_TRUE(strstr(el.msgs[0], "'#'") != NULL);
}

TEST(unknown_char_bang_alone) {
    /* '!' not followed by '=' — the two-char '!=' check fails,
       so '!' falls into the default: branch */
    el.count = 0;
    lex("!", toks, MAX_TOKENS, &el);
    ASSERT_TRUE(el.count > 0);
}

TEST(el_add_overflow_guard) {
    /* Exercises the  if (el->count >= MAX_ERRORS) return;  guard in
       el_add(): fill the error list to capacity before lexing,
       then confirm the count does NOT increase further. */
    el.count = MAX_ERRORS;
    lex("@", toks, MAX_TOKENS, &el);
    ASSERT_EQ(el.count, MAX_ERRORS);   /* must stay exactly at capacity */
}

TEST(too_many_tokens) {
    /* Feed MAX_TOKENS+4 semicolons — exercising the  n >= max_tokens-1
       guard inside the EMIT macro, which returns -1 and records an error. */
    int  need = MAX_TOKENS + 4;
    char *big = (char *)malloc((size_t)(need * 2 + 4));
    ASSERT_TRUE(big != NULL);
    int pos = 0;
    for (int i = 0; i < need; i++) { big[pos++] = ';'; big[pos++] = ' '; }
    big[pos] = '\0';
    el.count = 0;
    int ret = lex(big, toks, MAX_TOKENS, &el);
    free(big);
    ASSERT_TRUE(ret < 0);
    ASSERT_TRUE(el.count > 0);
    ASSERT_TRUE(strstr(el.msgs[0], "Too many tokens") != NULL);
}

/* ---- full tokenisation smoke test -------------------------------------- */
TEST(full_func_tokenise) {
    int n = do_lex("func add(a, b) { return a + b; }");
    ASSERT_EQ(el.count, 0);
    ASSERT_EQ(toks[0].type,  TOK_FUNC);
    ASSERT_EQ(toks[1].type,  TOK_IDENT);
    ASSERT_EQ(toks[2].type,  TOK_LPAREN);
    ASSERT_EQ(toks[3].type,  TOK_IDENT);    /* a */
    ASSERT_EQ(toks[4].type,  TOK_COMMA);
    ASSERT_EQ(toks[5].type,  TOK_IDENT);    /* b */
    ASSERT_EQ(toks[6].type,  TOK_RPAREN);
    ASSERT_EQ(toks[7].type,  TOK_LBRACE);
    ASSERT_EQ(toks[8].type,  TOK_RETURN);
    ASSERT_EQ(toks[9].type,  TOK_IDENT);    /* a */
    ASSERT_EQ(toks[10].type, TOK_PLUS);
    ASSERT_EQ(toks[11].type, TOK_IDENT);    /* b */
    ASSERT_EQ(toks[12].type, TOK_SEMICOLON);
    ASSERT_EQ(toks[13].type, TOK_RBRACE);
    ASSERT_EQ(n, 14);
}

/* =========================================================
 *  main
 * ========================================================= */
int main(void) {
    RUN(kw_func); RUN(kw_var); RUN(kw_if);
    RUN(kw_else); RUN(kw_while); RUN(kw_return);

    RUN(ident_simple); RUN(ident_underscore_start); RUN(ident_with_digits);
    RUN(kw_not_ident_prefix); RUN(ident_truncated_at_max);

    RUN(num_zero); RUN(num_decimal); RUN(num_max_255);
    RUN(num_hex_lower); RUN(num_hex_upper); RUN(num_hex_X);

    RUN(op_eq); RUN(op_neq); RUN(op_leq); RUN(op_geq);
    RUN(op_assign_vs_eq); RUN(op_lt_vs_leq); RUN(op_gt_vs_geq);
    RUN(ops_arith_all);

    RUN(punct_all);

    RUN(comment_mid_line); RUN(comment_to_eof_no_newline); RUN(comment_only);
    RUN(whitespace_skipped); RUN(eof_empty_source); RUN(line_numbers_track);

    RUN(unknown_char_at); RUN(unknown_char_hash); RUN(unknown_char_bang_alone);
    RUN(el_add_overflow_guard);
    RUN(too_many_tokens);

    RUN(full_func_tokenise);

    PRINT_RESULTS();
}
