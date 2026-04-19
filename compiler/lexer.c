/*
 * lexer.c — Tokeniser for VCL (C99)
 *
 * Converts raw source text into a flat Token array.
 * Supports: keywords, identifiers, decimal/hex integer literals,
 *           arithmetic and comparison operators, and // line comments.
 */

#include "compiler.h"
#include <ctype.h>

/* =========================================================
 *  Internal helpers
 * ========================================================= */

static void el_add(ErrorList *el, int line, const char *msg) {
    if (el->count >= MAX_ERRORS) return;
    snprintf(el->msgs[el->count], MAX_LINE, "Line %d: %s", line, msg);
    el->count++;
}

static const struct { const char *word; TokenType type; } KEYWORDS[] = {
    { "func",   TOK_FUNC   },
    { "var",    TOK_VAR    },
    { "if",     TOK_IF     },
    { "else",   TOK_ELSE   },
    { "while",  TOK_WHILE  },
    { "return", TOK_RETURN },
    { NULL,     TOK_ERROR  }
};

static TokenType keyword_or_ident(const char *s) {
    for (int i = 0; KEYWORDS[i].word; i++)
        if (strcmp(s, KEYWORDS[i].word) == 0)
            return KEYWORDS[i].type;
    return TOK_IDENT;
}

static Token make_tok(TokenType t, const char *text, int line) {
    Token tok;
    tok.type = t;
    tok.line = line;
    strncpy(tok.text, text, MAX_NAME - 1);
    tok.text[MAX_NAME - 1] = '\0';
    return tok;
}

/* =========================================================
 *  lex()
 *
 *  EMIT appends one token to the output buffer and returns -1 if full.
 *  It captures n, out, el, and line from the enclosing lex() call.
 * ========================================================= */

#define EMIT(t, txt)  do {                                           \
    if (n >= max_tokens - 1) {                                       \
        el_add(el, line, "Too many tokens");                         \
        return -1;                                                   \
    }                                                                \
    out[n++] = make_tok((t), (txt), line);                           \
} while (0)

int lex(const char *source, Token *out, int max_tokens, ErrorList *el) {
    int         n    = 0;
    int         line = 1;
    const char *p    = source;

    while (*p) {
        /* newline: advance line counter */
        if (*p == '\n') { line++; p++; continue; }

        /* other whitespace */
        if (isspace((unsigned char)*p)) { p++; continue; }

        /* line comment: skip to end of line */
        if (p[0] == '/' && p[1] == '/') {
            while (*p && *p != '\n') p++;
            continue;
        }

        /* identifiers and keywords */
        if (isalpha((unsigned char)*p) || *p == '_') {
            char buf[MAX_NAME];
            int  i = 0;
            while ((isalnum((unsigned char)*p) || *p == '_') && i < MAX_NAME - 1)
                buf[i++] = *p++;
            buf[i] = '\0';
            EMIT(keyword_or_ident(buf), buf);
            continue;
        }

        /* integer literals: decimal or 0x hex */
        if (isdigit((unsigned char)*p)) {
            char buf[MAX_NAME];
            int  i = 0;
            if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
                buf[i++] = *p++;
                buf[i++] = *p++;
                while (isxdigit((unsigned char)*p) && i < MAX_NAME - 1)
                    buf[i++] = *p++;
            } else {
                while (isdigit((unsigned char)*p) && i < MAX_NAME - 1)
                    buf[i++] = *p++;
            }
            buf[i] = '\0';
            EMIT(TOK_NUMBER, buf);
            continue;
        }

        /* two-character operators (must be checked before single-char) */
        if (p[0] == '=' && p[1] == '=') { EMIT(TOK_EQ,  "=="); p += 2; continue; }
        if (p[0] == '!' && p[1] == '=') { EMIT(TOK_NEQ, "!="); p += 2; continue; }
        if (p[0] == '<' && p[1] == '=') { EMIT(TOK_LEQ, "<="); p += 2; continue; }
        if (p[0] == '>' && p[1] == '=') { EMIT(TOK_GEQ, ">="); p += 2; continue; }

        /* single-character tokens */
        switch (*p) {
            case '+': EMIT(TOK_PLUS,       "+"); p++; break;
            case '-': EMIT(TOK_MINUS,      "-"); p++; break;
            case '&': EMIT(TOK_AMP,        "&"); p++; break;
            case '|': EMIT(TOK_PIPE,       "|"); p++; break;
            case '^': EMIT(TOK_CARET,      "^"); p++; break;
            case '<': EMIT(TOK_LT,         "<"); p++; break;
            case '>': EMIT(TOK_GT,         ">"); p++; break;
            case '=': EMIT(TOK_ASSIGN,     "="); p++; break;
            case '(': EMIT(TOK_LPAREN,     "("); p++; break;
            case ')': EMIT(TOK_RPAREN,     ")"); p++; break;
            case '{': EMIT(TOK_LBRACE,     "{"); p++; break;
            case '}': EMIT(TOK_RBRACE,     "}"); p++; break;
            case ',': EMIT(TOK_COMMA,      ","); p++; break;
            case ';': EMIT(TOK_SEMICOLON,  ";"); p++; break;
            default: {
                char msg[64];
                snprintf(msg, sizeof(msg), "Unexpected character '%c'", *p);
                el_add(el, line, msg);
                p++;
                break;
            }
        }
    }

    out[n] = make_tok(TOK_EOF, "", line);
    return n;
}

#undef EMIT
