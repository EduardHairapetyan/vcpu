/*
 * parser.c — Recursive descent parser for VCL (C99)
 *
 * Grammar (simplified):
 *
 *   program     := (func_decl | var_decl)*
 *   func_decl   := 'func' IDENT '(' param_list? ')' block
 *   param_list  := IDENT (',' IDENT)*
 *   var_decl    := 'var' IDENT ('=' expr)? ';'
 *   block       := '{' stmt* '}'
 *   stmt        := var_decl | assign_stmt | if_stmt
 *                | while_stmt | return_stmt | call_stmt
 *   assign_stmt := IDENT '=' expr ';'
 *   if_stmt     := 'if' '(' expr ')' block ('else' block)?
 *   while_stmt  := 'while' '(' expr ')' block
 *   return_stmt := 'return' expr ';'
 *   call_stmt   := IDENT '(' arg_list? ')' ';'
 *   expr        := comparison
 *   comparison  := addition (CMP_OP addition)?
 *   addition    := primary (('+' | '-' | '&' | '|' | '^') primary)*
 *   primary     := NUMBER | IDENT | call_expr | '(' expr ')'
 *   call_expr   := IDENT '(' arg_list? ')'
 *   arg_list    := expr (',' expr)*
 */

#include "compiler.h"

/* =========================================================
 *  AST node pool
 * ========================================================= */

static ASTNode node_pool[MAX_NODES];
static int     pool_top = 0;

void ast_reset(void) { pool_top = 0; }

static ASTNode *node_new(NodeType type, int line) {
    if (pool_top >= MAX_NODES) return NULL;
    ASTNode *n = &node_pool[pool_top++];
    memset(n, 0, sizeof(*n));
    n->type = type;
    n->line = line;
    return n;
}

/* Set to 1 by node_add_child if MAX_CHILDREN is exceeded.
   Checked by parse() before returning.                         */
static int g_child_overflow = 0;

static void node_add_child(ASTNode *parent, ASTNode *child) {
    if (parent->nchildren < MAX_CHILDREN) {
        parent->children[parent->nchildren++] = child;
    } else {
        /* Block or program node has too many children.
           Record the overflow so parse() can report it. */
        g_child_overflow = 1;
    }
}

/* =========================================================
 *  Parser state
 * ========================================================= */

typedef struct {
    const Token   *tokens;
    int            pos;
    ErrorList *el;
} Parser;

static const Token *peek(Parser *p)    { return &p->tokens[p->pos];     }
static const Token *advance(Parser *p) { return &p->tokens[p->pos++];   }
static int check(Parser *p, TokenType t) { return peek(p)->type == t;   }

static int match(Parser *p, TokenType t) {
    if (!check(p, t)) return 0;
    advance(p);
    return 1;
}

static const Token *expect(Parser *p, TokenType t, const char *what) {
    if (check(p, t)) return advance(p);
    if (p->el->count < MAX_ERRORS) {
        snprintf(p->el->msgs[p->el->count++], MAX_LINE,
                 "Line %d: Expected %s but got '%.60s'",
                 peek(p)->line, what, peek(p)->text);
    }
    /* return current token so parsing can attempt to continue */
    return peek(p);
}

static void parse_err(Parser *p, const char *msg) {
    if (p->el->count < MAX_ERRORS) {
        snprintf(p->el->msgs[p->el->count++], MAX_LINE,
                 "Line %d: %s", peek(p)->line, msg);
    }
}

/* =========================================================
 *  Forward declarations
 * ========================================================= */

static ASTNode *parse_expr(Parser *p);
static ASTNode *parse_stmt(Parser *p);
static ASTNode *parse_block(Parser *p);

/* =========================================================
 *  Expressions
 * ========================================================= */

/* Parse argument list shared by call expressions and call statements */
static void parse_args(Parser *p, ASTNode *call_node) {
    while (!check(p, TOK_RPAREN) && !check(p, TOK_EOF)) {
        ASTNode *arg = parse_expr(p);
        if (arg) node_add_child(call_node, arg);
        if (!check(p, TOK_RPAREN))
            expect(p, TOK_COMMA, "','");
    }
    expect(p, TOK_RPAREN, "')'");
}

static ASTNode *parse_primary(Parser *p) {
    const Token *t = peek(p);

    /* integer literal */
    if (t->type == TOK_NUMBER) {
        advance(p);
        ASTNode *n = node_new(NODE_NUMBER, t->line);
        if (!n) return NULL;
        n->ival = (int)strtol(t->text, NULL, 0);
        if (n->ival < 0 || n->ival > 255) {
            char msg[MAX_LINE];
            snprintf(msg, MAX_LINE,
                     "Integer literal %d out of range 0-255", n->ival);
            parse_err(p, msg);
        }
        return n;
    }

    /* identifier or function call */
    if (t->type == TOK_IDENT) {
        advance(p);
        if (check(p, TOK_LPAREN)) {
            advance(p);
            ASTNode *n = node_new(NODE_CALL, t->line);
            if (!n) return NULL;
            strncpy(n->name, t->text, MAX_NAME - 1);
            parse_args(p, n);
            return n;
        }
        ASTNode *n = node_new(NODE_IDENT, t->line);
        if (!n) return NULL;
        strncpy(n->name, t->text, MAX_NAME - 1);
        return n;
    }

    /* parenthesised sub-expression */
    if (t->type == TOK_LPAREN) {
        advance(p);
        ASTNode *n = parse_expr(p);
        expect(p, TOK_RPAREN, "')'");
        return n;
    }

    char msg[MAX_LINE];
    snprintf(msg, MAX_LINE, "Unexpected token '%s' in expression", t->text);
    parse_err(p, msg);
    return NULL;
}

/* Left-associative chain: primary (op primary)* */
static int is_arith(TokenType t) {
    return t == TOK_PLUS  || t == TOK_MINUS || t == TOK_AMP ||
           t == TOK_PIPE  || t == TOK_CARET;
}

static ASTNode *parse_addition(Parser *p) {
    ASTNode *left = parse_primary(p);
    if (!left) return NULL;

    while (is_arith(peek(p)->type)) {
        const Token *op = advance(p);
        ASTNode *right = parse_primary(p);
        if (!right) return NULL;
        ASTNode *n = node_new(NODE_BINOP, op->line);
        if (!n) return NULL;
        n->ival        = (int)(unsigned char)op->text[0]; /* '+' '-' '&' '|' '^' */
        n->children[0] = left;
        n->children[1] = right;
        n->nchildren   = 2;
        left = n;
    }
    return left;
}

static int is_cmp(TokenType t) {
    return t == TOK_EQ || t == TOK_NEQ || t == TOK_LT ||
           t == TOK_GT || t == TOK_LEQ || t == TOK_GEQ;
}

static ASTNode *parse_expr(Parser *p) {
    ASTNode *left = parse_addition(p);
    if (!left) return NULL;
    if (!is_cmp(peek(p)->type)) return left;

    const Token *op = advance(p);
    ASTNode *right = parse_addition(p);
    if (!right) return NULL;

    ASTNode *n = node_new(NODE_BINOP, op->line);
    if (!n) return NULL;
    /* is_cmp() guards entry to this branch, so all reachable op types are
       handled.  The switch is intentionally non-exhaustive. */
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"
#endif
    switch (op->type) {
        case TOK_EQ:  n->ival = BINOP_EQ;  break;
        case TOK_NEQ: n->ival = BINOP_NEQ; break;
        case TOK_LEQ: n->ival = BINOP_LEQ; break;
        case TOK_GEQ: n->ival = BINOP_GEQ; break;
        case TOK_LT:  n->ival = '<';       break;
        case TOK_GT:  n->ival = '>';       break;
    }
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
    n->children[0] = left;
    n->children[1] = right;
    n->nchildren   = 2;
    return n;
}

/* =========================================================
 *  Statements
 * ========================================================= */

static ASTNode *parse_var_decl(Parser *p) {
    /* 'var' has already been consumed by the caller */
    const Token *name = expect(p, TOK_IDENT, "variable name");
    ASTNode *n = node_new(NODE_VAR_DECL, name->line);
    if (!n) return NULL;
    strncpy(n->name, name->text, MAX_NAME - 1);
    if (match(p, TOK_ASSIGN)) {
        ASTNode *init = parse_expr(p);
        if (init) node_add_child(n, init);
    }
    expect(p, TOK_SEMICOLON, "';'");
    return n;
}

static ASTNode *parse_if(Parser *p) {
    /* 'if' already consumed */
    ASTNode *n = node_new(NODE_IF, peek(p)->line);
    if (!n) return NULL;
    expect(p, TOK_LPAREN, "'('");
    ASTNode *cond = parse_expr(p);
    expect(p, TOK_RPAREN, "')'");
    ASTNode *then = parse_block(p);
    if (cond) node_add_child(n, cond);
    if (then) node_add_child(n, then);
    if (match(p, TOK_ELSE)) {
        ASTNode *els = parse_block(p);
        if (els) node_add_child(n, els);
    }
    return n;
}

static ASTNode *parse_while(Parser *p) {
    /* 'while' already consumed */
    ASTNode *n = node_new(NODE_WHILE, peek(p)->line);
    if (!n) return NULL;
    expect(p, TOK_LPAREN, "'('");
    ASTNode *cond = parse_expr(p);
    expect(p, TOK_RPAREN, "')'");
    ASTNode *body = parse_block(p);
    if (cond) node_add_child(n, cond);
    if (body) node_add_child(n, body);
    return n;
}

static ASTNode *parse_stmt(Parser *p) {
    const Token *t = peek(p);

    if (t->type == TOK_VAR)    { advance(p); return parse_var_decl(p); }
    if (t->type == TOK_IF)     { advance(p); return parse_if(p);       }
    if (t->type == TOK_WHILE)  { advance(p); return parse_while(p);    }

    if (t->type == TOK_RETURN) {
        advance(p);
        ASTNode *n = node_new(NODE_RETURN, t->line);
        if (!n) return NULL;
        ASTNode *val = parse_expr(p);
        if (val) node_add_child(n, val);
        expect(p, TOK_SEMICOLON, "';'");
        return n;
    }

    /* assignment or call statement — both start with an identifier */
    if (t->type == TOK_IDENT) {
        const Token *name = advance(p);

        /* call statement: ident '(' args ')' ';' */
        if (check(p, TOK_LPAREN)) {
            advance(p);
            ASTNode *n = node_new(NODE_CALL, name->line);
            if (!n) return NULL;
            strncpy(n->name, name->text, MAX_NAME - 1);
            parse_args(p, n);
            expect(p, TOK_SEMICOLON, "';'");
            return n;
        }

        /* assignment statement: ident '=' expr ';' */
        expect(p, TOK_ASSIGN, "'='");
        ASTNode *rhs = parse_expr(p);
        expect(p, TOK_SEMICOLON, "';'");
        ASTNode *n = node_new(NODE_ASSIGN, name->line);
        if (!n) return NULL;
        strncpy(n->name, name->text, MAX_NAME - 1);
        if (rhs) node_add_child(n, rhs);
        return n;
    }

    char msg[MAX_LINE];
    snprintf(msg, MAX_LINE, "Unexpected token '%s' in statement", t->text);
    parse_err(p, msg);
    advance(p); /* skip bad token to avoid infinite loop */
    return NULL;
}

static ASTNode *parse_block(Parser *p) {
    const Token *brace = expect(p, TOK_LBRACE, "'{'");
    ASTNode *block = node_new(NODE_BLOCK, brace->line);
    if (!block) return NULL;
    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
        ASTNode *s = parse_stmt(p);
        if (s) node_add_child(block, s);
        if (p->el->count > 0) break;
    }
    expect(p, TOK_RBRACE, "'}'");
    return block;
}

/* =========================================================
 *  Top-level: func and var declarations
 * ========================================================= */

static ASTNode *parse_func(Parser *p) {
    /* 'func' already consumed */
    const Token *name = expect(p, TOK_IDENT, "function name");
    ASTNode *n = node_new(NODE_FUNC, name->line);
    if (!n) return NULL;
    strncpy(n->name, name->text, MAX_NAME - 1);

    expect(p, TOK_LPAREN, "'('");
    while (!check(p, TOK_RPAREN) && !check(p, TOK_EOF)) {
        const Token *param = expect(p, TOK_IDENT, "parameter name");
        if (n->nparams < MAX_PARAMS)
            strncpy(n->params[n->nparams++], param->text, MAX_NAME - 1);
        else
            parse_err(p, "Function has more than 3 parameters");
        if (!check(p, TOK_RPAREN))
            expect(p, TOK_COMMA, "','");
    }
    expect(p, TOK_RPAREN, "')'");

    ASTNode *body = parse_block(p);
    if (body) node_add_child(n, body);
    return n;
}

/* =========================================================
 *  parse() — entry point
 * ========================================================= */

ASTNode *parse(const Token *tokens, ErrorList *el) {
    Parser p;
    p.tokens = tokens;
    p.pos    = 0;
    p.el     = el;
    g_child_overflow = 0;

    ASTNode *root = node_new(NODE_PROGRAM, 1);
    if (!root) return NULL;

    while (!check(&p, TOK_EOF) && el->count == 0) {
        if      (match(&p, TOK_FUNC)) { 
            ASTNode *f = parse_func(&p);
            if (f) 
                node_add_child(root, f);
        }
        else if (match(&p, TOK_VAR))  { ASTNode *v = parse_var_decl(&p); if (v) node_add_child(root, v); }
        else {
            char msg[MAX_LINE];
            snprintf(msg, MAX_LINE,
                     "Expected 'func' or 'var' at top level, got '%s'",
                     peek(&p)->text);
            parse_err(&p, msg);
        }
    }

    if (g_child_overflow && el->count < MAX_ERRORS) {
        snprintf(el->msgs[el->count++], MAX_LINE,
                 "AST node exceeded MAX_CHILDREN (%d) — "
                 "too many statements or top-level declarations",
                 MAX_CHILDREN);
    }
    return root;
}
