/*
 * codegen.c — AST walker that emits vCPU assembly text (C99)
 *
 * Strategy:
 *   - All expressions evaluate into R0.
 *   - Binary ops: evaluate right → R0, PUSH R0, evaluate left → R0,
 *     POP R1, then operate on R0 and R1.  This is pure stack-based
 *     codegen — correct for arbitrary expression depth, call-safe
 *     because intermediate values live on the stack between PUSH/POP.
 *   - Comparisons used as conditions emit a CMP + conditional jump pair
 *     directly; they never materialise a 0/1 value into a register.
 *   - Global and local variables are statically assigned data-memory
 *     addresses at compile time.  Functions share an address space but
 *     each function's locals occupy a disjoint slice, so non-recursive
 *     programs work correctly.
 *   - CALL/RET manage the return address; the stack also holds
 *     expression temporaries and arguments.  Callee saves nothing
 *     because the simple codegen never keeps live values in registers
 *     across a function call.
 *
 * Output format: plain text accepted by the existing assembler.
 *   Labels:        name:
 *   Instructions:  four-space indent, upper-case mnemonic
 *   Comments:      ; text
 */

#include "compiler.h"
#include <stdarg.h>

/* =========================================================
 *  Types
 * ========================================================= */

typedef struct {
    char *buf;
    int   pos;
    int   sz;
    int   overflow;
} OutBuf;

typedef enum { SYM_GLOBAL, SYM_LOCAL, SYM_PARAM, SYM_FUNC } SymKind;

typedef struct {
    char    name[MAX_NAME];
    int     addr;
    SymKind kind;
} Symbol;

typedef struct {
    Symbol entries[MAX_SYMS];
    int    count;
} SymTable;

typedef struct {
    SymTable      globals;
    SymTable      locals;     /* cleared and rebuilt for each function */
    int           next_addr;  /* monotone address allocator            */
    int           label_cnt;  /* for unique label generation           */
    int           in_func;    /* 1 when inside a function body         */
    OutBuf       *out;
    ErrorList    *el;
} CG;

/* =========================================================
 *  Helper functions
 * ========================================================= */

static void out_printf(OutBuf *b, const char *fmt, ...) {
    if (b->overflow) return;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(b->buf + b->pos, (size_t)(b->sz - b->pos), fmt, ap);
    va_end(ap);
    if (n < 0 || n >= b->sz - b->pos)
        b->overflow = 1;
    else
        b->pos += n;
}

static Symbol *sym_find(SymTable *t, const char *name) {
    for (int i = 0; i < t->count; i++)
        if (strcmp(t->entries[i].name, name) == 0)
            return &t->entries[i];
    return NULL;
}

static int sym_add(SymTable *t, const char *name, int addr, SymKind kind) {
    if (t->count >= MAX_SYMS) return 0;
    strncpy(t->entries[t->count].name, name, MAX_NAME - 1);
    t->entries[t->count].name[MAX_NAME - 1] = '\0';
    t->entries[t->count].addr = addr;
    t->entries[t->count].kind = kind;
    t->count++;
    return 1;
}

static void cg_err(CG *cg, int line, const char *msg) {
    if (cg->el->count < MAX_ERRORS) {
        snprintf(cg->el->msgs[cg->el->count++], MAX_LINE,
                 "Line %d: %s", line, msg);
    }
}

/* Look up a variable name: locals shadow globals */
static Symbol *cg_lookup(CG *cg, const char *name) {
    Symbol *s = sym_find(&cg->locals, name);
    if (s) return s;
    return sym_find(&cg->globals, name);
}

static int cg_alloc(CG *cg) {
    if (cg->next_addr >= COMP_MEM_LIMIT) return -1;
    return cg->next_addr++;
}

/* =========================================================
 *  Forward declarations
 * ========================================================= */

static void gen_expr(CG *cg, const ASTNode *n);
static void gen_stmt(CG *cg, const ASTNode *n);
static void gen_block(CG *cg, const ASTNode *n);

/* =========================================================
 *  Condition code generation
 *
 *  gen_jump_if_true / gen_jump_if_false emit the CMP and the
 *  conditional jump(s) needed to branch to `lbl` depending on
 *  whether the comparison result is true or false.
 *
 *  After evaluating left and right into R0 and R1:
 *    CMP R0, R1  computes  R0 - R1  and sets flags:
 *      Z=1  if R0 == R1
 *      C=1  if R0  < R1  (unsigned borrow)
 *      S=1  if bit-7 of result is set
 *
 *  For '>' and '>=' we swap operands so the < / <= logic applies.
 * ========================================================= */

static void gen_cmp_setup(CG *cg, const ASTNode *node, int *swapped) {
    int op = node->ival;
    *swapped = (op == '>' || op == BINOP_GEQ);
    if (*swapped) {
        /* swap: evaluate right first, then left, CMP right-R0, left-R1 */
        gen_expr(cg, node->children[0]); /* left  → R0 */
        out_printf(cg->out, "    PUSH R0\n");
        gen_expr(cg, node->children[1]); /* right → R0 */
        out_printf(cg->out, "    POP R1\n");
        /* now R0=right, R1=left; CMP R0,R1 = right - left */
    } else {
        gen_expr(cg, node->children[1]); /* right → R0 */
        out_printf(cg->out, "    PUSH R0\n");
        gen_expr(cg, node->children[0]); /* left  → R0 */
        out_printf(cg->out, "    POP R1\n");
        /* now R0=left, R1=right; CMP R0,R1 = left - right */
    }
    out_printf(cg->out, "    CMP R0, R1\n");
}

/* Jump to lbl if comparison is TRUE */
static void gen_jump_if_true(CG *cg, const ASTNode *cond, int lbl) {
    int swapped;
    gen_cmp_setup(cg, cond, &swapped);
    int effective_op = cond->ival;
    if (swapped && effective_op == '>')       effective_op = '<';
    if (swapped && effective_op == BINOP_GEQ) effective_op = BINOP_LEQ;

    switch (effective_op) {
        case BINOP_EQ:
            out_printf(cg->out, "    JUMP_ZERO _L%d\n", lbl);
            break;
        case BINOP_NEQ:
            out_printf(cg->out, "    JUMP_NOT_ZERO _L%d\n", lbl);
            break;
        case '<':
            /* C=1 when left < right (unsigned borrow) */
            out_printf(cg->out, "    JUMP_CARRY _L%d\n", lbl);
            break;
        case BINOP_LEQ:
            /* left <= right: either Z=1 (equal) or C=1 (less) */
            out_printf(cg->out, "    JUMP_ZERO _L%d\n", lbl);
            out_printf(cg->out, "    JUMP_CARRY _L%d\n", lbl);
            break;
        /* All six comparison ops are handled above; no default needed. */
    }
}

/* Jump to lbl if comparison is FALSE */
static void gen_jump_if_false(CG *cg, const ASTNode *cond, int lbl) {
    int swapped;
    gen_cmp_setup(cg, cond, &swapped);
    int effective_op = cond->ival;
    if (swapped && effective_op == '>')       effective_op = '<';
    if (swapped && effective_op == BINOP_GEQ) effective_op = BINOP_LEQ;

    switch (effective_op) {
        case BINOP_EQ:
            out_printf(cg->out, "    JUMP_NOT_ZERO _L%d\n", lbl);
            break;
        case BINOP_NEQ:
            out_printf(cg->out, "    JUMP_ZERO _L%d\n", lbl);
            break;
        case '<':
            /* false when left >= right: C=0 */
            out_printf(cg->out, "    JUMP_NOT_CARRY _L%d\n", lbl);
            break;
        case BINOP_LEQ: {
            /* false when left > right: Z=0 AND C=0
               Emit: skip jump-to-lbl if Z=1 or C=1, otherwise jump */
            int skip = cg->label_cnt++;
            out_printf(cg->out, "    JUMP_ZERO _L%d\n",      skip);
            out_printf(cg->out, "    JUMP_CARRY _L%d\n",     skip);
            out_printf(cg->out, "    JUMP _L%d\n",           lbl);
            out_printf(cg->out, "_L%d:\n",                   skip);
            break;
        }
        /* All six comparison ops are handled above; no default needed. */
    }
}

/* =========================================================
 *  Expression code generation
 *  All paths leave their result in R0.
 * ========================================================= */

static void gen_call_expr(CG *cg, const ASTNode *n);

static void gen_expr(CG *cg, const ASTNode *n) {
    if (!n) return;

    /* gen_expr is only called with expression nodes (NUMBER, IDENT, BINOP,
       CALL).  The switch is intentionally non-exhaustive: statement and
       declaration node types never reach here. */
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"
#endif
    switch (n->type) {

        case NODE_NUMBER:
            out_printf(cg->out, "    MOV R0, %d\n", n->ival & 0xFF);
            break;

        case NODE_IDENT: {
            Symbol *s = cg_lookup(cg, n->name);
            out_printf(cg->out, "    LOAD R0, %d   ; %s\n", s->addr, n->name);
            break;
        }

        case NODE_BINOP: {
            int op = n->ival;

            /* comparison operators: materialise result as 0 or 1 */
            if (op == BINOP_EQ || op == BINOP_NEQ || op == BINOP_LEQ ||
                op == BINOP_GEQ || op == '<' || op == '>') {
                int lbl_true = cg->label_cnt++;
                int lbl_end  = cg->label_cnt++;
                gen_jump_if_true(cg, n, lbl_true);
                out_printf(cg->out, "    MOV R0, 0\n");
                out_printf(cg->out, "    JUMP _L%d\n",  lbl_end);
                out_printf(cg->out, "_L%d:\n",          lbl_true);
                out_printf(cg->out, "    MOV R0, 1\n");
                out_printf(cg->out, "_L%d:\n",          lbl_end);
                break;
            }

            /* arithmetic / bitwise: evaluate right, push, evaluate left, pop R1, op */
            gen_expr(cg, n->children[1]);
            out_printf(cg->out, "    PUSH R0\n");
            gen_expr(cg, n->children[0]);
            out_printf(cg->out, "    POP R1\n");
            switch (op) {
                case '+':  out_printf(cg->out, "    ADD R0, R1\n"); break;
                case '-':  out_printf(cg->out, "    SUB R0, R1\n"); break;
                case '&':  out_printf(cg->out, "    AND R0, R1\n"); break;
                case '|':  out_printf(cg->out, "    OR R0, R1\n");  break;
                case '^':  out_printf(cg->out, "    XOR R0, R1\n"); break;
                /* Only arithmetic ops reach here; all cases are covered. */
            }
            break;
        }

        case NODE_CALL:
            gen_call_expr(cg, n);
            /* return value already in R0 */
            break;

        /* gen_expr is only called with NUMBER, IDENT, BINOP, CALL nodes. */
    }
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
}

/* =========================================================
 *  Function call codegen
 *  Arguments are pushed right-to-left so the callee can pop
 *  them left-to-right (first arg is on top when CALL executes).
 *  CALL then pushes the return address on top of the args.
 *  The callee's prologue saves/restores the return address via R7
 *  and pops each argument directly into its data-memory slot.
 * ========================================================= */

static void gen_call_expr(CG *cg, const ASTNode *n) {
    int nargs = n->nchildren;
    /* push arguments right-to-left; callee pops them in forward order */
    for (int i = nargs - 1; i >= 0; i--) {
        gen_expr(cg, n->children[i]);
        out_printf(cg->out, "    PUSH R0\n");
    }
    out_printf(cg->out, "    CALL %s\n", n->name);
    /* return value is now in R0 */
}

/* =========================================================
 *  Statement code generation
 * ========================================================= */

static void gen_stmt(CG *cg, const ASTNode *n) {
    if (!n) return;

    /* gen_stmt is only called on nodes the parser places inside a block:
       VAR_DECL, ASSIGN, IF, WHILE, RETURN, CALL.  The switch is
       intentionally non-exhaustive for expression and top-level nodes. */
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"
#endif
    switch (n->type) {

        case NODE_VAR_DECL: {
            SymTable *tbl = cg->in_func ? &cg->locals : &cg->globals;
            int addr = cg_alloc(cg);
            if (addr < 0) { cg_err(cg, n->line, "Out of data memory"); return; }
            sym_add(tbl, n->name, addr, SYM_LOCAL);
            if (n->nchildren > 0) {
                gen_expr(cg, n->children[0]);
                out_printf(cg->out, "    STORE R0, %d   ; var %s\n", addr, n->name);
            } else {
                out_printf(cg->out, "    MOV R0, 0\n");
                out_printf(cg->out, "    STORE R0, %d   ; var %s\n", addr, n->name);
            }
            break;
        }

        case NODE_ASSIGN: {
            Symbol *s = cg_lookup(cg, n->name);
            gen_expr(cg, n->children[0]);
            out_printf(cg->out, "    STORE R0, %d   ; %s\n", s->addr, n->name);
            break;
        }

        case NODE_IF: {
            int lbl_else = cg->label_cnt++;
            int lbl_end  = cg->label_cnt++;
            /* condition → jump to else if false */
            gen_jump_if_false(cg, n->children[0], lbl_else);
            /* then block */
            gen_block(cg, n->children[1]);
            if (n->nchildren >= 3) {
                out_printf(cg->out, "    JUMP _L%d\n", lbl_end);
                out_printf(cg->out, "_L%d:\n", lbl_else);
                gen_block(cg, n->children[2]);
                out_printf(cg->out, "_L%d:\n", lbl_end);
            } else {
                out_printf(cg->out, "_L%d:\n", lbl_else);
            }
            break;
        }

        case NODE_WHILE: {
            int lbl_top = cg->label_cnt++;
            int lbl_end = cg->label_cnt++;
            out_printf(cg->out, "_L%d:\n", lbl_top);
            gen_jump_if_false(cg, n->children[0], lbl_end);
            gen_block(cg, n->children[1]);
            out_printf(cg->out, "    JUMP _L%d\n", lbl_top);
            out_printf(cg->out, "_L%d:\n", lbl_end);
            break;
        }

        case NODE_RETURN:
            gen_expr(cg, n->children[0]);
            /* return value is in R0; emit RET */
            out_printf(cg->out, "    RET\n");
            break;

        case NODE_CALL:
            gen_call_expr(cg, n);
            /* discard return value — result in R0 is simply unused */
            break;

        /* gen_stmt is only called on nodes the parser places inside a block:
           VAR_DECL, ASSIGN, IF, WHILE, RETURN, CALL.  All cases are covered. */
    }
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
}

static void gen_block(CG *cg, const ASTNode *n) {
    if (!n) return;
    for (int i = 0; i < n->nchildren; i++)
        gen_stmt(cg, n->children[i]);
}

/* =========================================================
 *  Function code generation
 * ========================================================= */

static void gen_func(CG *cg, const ASTNode *n) {
    /* each function gets a fresh local symbol table */
    cg->locals.count = 0;
    cg->in_func      = 1;

    out_printf(cg->out, "\n%s:\n", n->name);

    /* Pop incoming arguments from the stack into data memory.
     * CALL pushed the return address last, so save it in R7 first,
     * then pop each parameter left-to-right into its slot, then
     * restore the return address so RET finds it on top of the stack. */
    if (n->nparams > 0) {
        out_printf(cg->out, "    POP R7\n");   /* save return address */
        for (int i = 0; i < n->nparams; i++) {
            int addr = cg_alloc(cg);
            if (addr < 0) { cg_err(cg, n->line, "Out of data memory"); return; }
            sym_add(&cg->locals, n->params[i], addr, SYM_PARAM);
            out_printf(cg->out, "    POP R0\n");
            out_printf(cg->out, "    STORE R0, %d   ; param %s\n",
                       addr, n->params[i]);
        }
        out_printf(cg->out, "    PUSH R7\n");  /* restore return address */
    }

    /* generate function body */
    if (n->nchildren > 0)
        gen_block(cg, n->children[0]);

    /* implicit return 0 if execution falls off the end */
    out_printf(cg->out, "    MOV R0, 0\n");
    out_printf(cg->out, "    RET\n");
}

/* =========================================================
 *  codegen() — entry point
 * ========================================================= */

int codegen(const ASTNode *root, char *out_buf, int out_sz, ErrorList *el) {
    OutBuf ob;
    ob.buf      = out_buf;
    ob.pos      = 0;
    ob.sz       = out_sz;
    ob.overflow = 0;
    ob.buf[0]   = '\0';

    CG cg;
    memset(&cg, 0, sizeof(cg));
    cg.next_addr = 0;
    cg.label_cnt = 0;
    cg.out       = &ob;
    cg.el        = el;

    /* ---- pass 0: register function names --------------------------------
       Insert every NODE_FUNC name into the global symbol table so that
       forward and mutual calls can be validated before bodies are emitted. */
    for (int i = 0; i < root->nchildren; i++) {
        const ASTNode *decl = root->children[i];
        if (decl->type == NODE_FUNC)
            sym_add(&cg.globals, decl->name, 0, SYM_FUNC);
    }

    /* ---- pass 1: allocate globals ----------------------------------------
       Walk only VAR_DECL children of the program node so that global
       addresses are stable before any function body is emitted.          */
    for (int i = 0; i < root->nchildren; i++) {
        const ASTNode *decl = root->children[i];
        if (decl->type == NODE_VAR_DECL) {
            int addr = cg_alloc(&cg);
            if (addr < 0) { cg_err(&cg, decl->line, "Out of data memory"); return 0; }
            sym_add(&cg.globals, decl->name, addr, SYM_GLOBAL);
        }
    }

    /* ---- emit __start: initialise globals, call main, END --------------- */
    out_printf(&ob, "; Generated by VCL compiler\n");
    out_printf(&ob, "__start:\n");

    for (int i = 0; i < root->nchildren; i++) {
        const ASTNode *decl = root->children[i];
        if (decl->type != NODE_VAR_DECL) continue;
        Symbol *s = sym_find(&cg.globals, decl->name);
        if (!s) continue;
        if (decl->nchildren > 0) {
            /* temporarily use globals-only context for the init expression */
            cg.locals.count = 0;
            gen_expr(&cg, decl->children[0]);
            out_printf(&ob, "    STORE R0, %d   ; global %s\n", s->addr, decl->name);
        } else {
            out_printf(&ob, "    MOV R0, 0\n");
            out_printf(&ob, "    STORE R0, %d   ; global %s\n", s->addr, decl->name);
        }
    }

    out_printf(&ob, "    CALL main\n");
    out_printf(&ob, "    END\n");

    /* ---- emit each function --------------------------------------------- */
    for (int i = 0; i < root->nchildren; i++) {
        const ASTNode *decl = root->children[i];
        if (decl->type == NODE_FUNC)
            gen_func(&cg, decl);
    }

    if (ob.overflow) {
        if (el->count < MAX_ERRORS)
            snprintf(el->msgs[el->count++], MAX_LINE,
                     "Output buffer too small");
        return 0;
    }
    return el->count == 0;
}

/* =========================================================
 *  compile() — high-level entry point
 * ========================================================= */

int compile(const char *source, char *out_buf, int out_sz, ErrorList *el) {
    Token tokens[MAX_TOKENS];
    ast_reset();

    int ntok = lex(source, tokens, MAX_TOKENS, el);
    if (ntok < 0 || el->count > 0) return 0;

    ASTNode *root = parse(tokens, el);
    if (!root || el->count > 0) return 0;

    if (!semantic(root, el)) return 0;

    return codegen(root, out_buf, out_sz, el);
}
