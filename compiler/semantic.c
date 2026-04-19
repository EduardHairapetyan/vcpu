/*
 * semantic.c — Semantic analysis pass for the VCL compiler (C99)
 *
 * Walks the AST after parse() and before codegen(), checking:
 *   - All variables and functions are declared before use
 *   - Function names are not used as variables (NODE_IDENT → SYM_FUNC)
 *   - Variable names are not called as functions (NODE_CALL → non-SYM_FUNC)
 *   - Function calls have the correct argument count
 *   - No duplicate declarations in the same scope
 *
 * semantic() builds its own internal symbol table (separate from codegen's).
 * SEM_FUNC entries store nparams so argument count can be validated.
 */

#include "compiler.h"
#include <string.h>
#include <stdio.h>

/* =========================================================
 *  Internal symbol table
 * ========================================================= */

typedef enum { SEM_VAR, SEM_FUNC } SemKind;

typedef struct {
    char    name[MAX_NAME];
    SemKind kind;
    int     nparams;   /* for SEM_FUNC: declared parameter count */
} SemSym;

typedef struct {
    SemSym entries[MAX_SYMS];
    int    count;
} SemTable;

typedef struct {
    SemTable   globals;   /* functions and global variables */
    ErrorList *el;
} SemCtx;

/* =========================================================
 *  Helper functions
 * ========================================================= */

static SemSym *sem_find(SemTable *t, const char *name) {
    for (int i = 0; i < t->count; i++)
        if (strcmp(t->entries[i].name, name) == 0)
            return &t->entries[i];
    return NULL;
}

static void sem_add(SemTable *t, const char *name, SemKind kind, int nparams) {
    if (t->count >= MAX_SYMS) return;
    strncpy(t->entries[t->count].name, name, MAX_NAME - 1);
    t->entries[t->count].name[MAX_NAME - 1] = '\0';
    t->entries[t->count].kind    = kind;
    t->entries[t->count].nparams = nparams;
    t->count++;
}

static void sem_err(SemCtx *ctx, int line, const char *msg) {
    if (ctx->el->count < MAX_ERRORS)
        snprintf(ctx->el->msgs[ctx->el->count++], MAX_LINE,
                 "Line %d: %s", line, msg);
}

/* Look up a name: locals shadow globals */
static SemSym *sem_lookup(SemCtx *ctx, SemTable *locals, const char *name) {
    SemSym *s = sem_find(locals, name);
    if (s) return s;
    return sem_find(&ctx->globals, name);
}

/* =========================================================
 *  Forward declarations
 * ========================================================= */

static void sem_expr(SemCtx *ctx, SemTable *locals, const ASTNode *n);
static void sem_stmt(SemCtx *ctx, SemTable *locals, const ASTNode *n);
static void sem_block(SemCtx *ctx, SemTable *locals, const ASTNode *n);

/* =========================================================
 *  Expression walker
 * ========================================================= */

static void sem_expr(SemCtx *ctx, SemTable *locals, const ASTNode *n) {
    if (!n) return;

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"
#endif
    switch (n->type) {

        case NODE_NUMBER:
            break;

        case NODE_IDENT: {
            SemSym *s = sem_lookup(ctx, locals, n->name);
            if (!s) {
                char msg[MAX_LINE];
                snprintf(msg, MAX_LINE, "Undefined variable '%s'", n->name);
                sem_err(ctx, n->line, msg);
            } else if (s->kind == SEM_FUNC) {
                char msg[MAX_LINE];
                snprintf(msg, MAX_LINE,
                         "'%s' is a function, not a variable", n->name);
                sem_err(ctx, n->line, msg);
            }
            break;
        }

        case NODE_CALL: {
            SemSym *s = sem_lookup(ctx, locals, n->name);
            if (!s) {
                char msg[MAX_LINE];
                snprintf(msg, MAX_LINE, "Undefined function '%s'", n->name);
                sem_err(ctx, n->line, msg);
            } else if (s->kind != SEM_FUNC) {
                char msg[MAX_LINE];
                snprintf(msg, MAX_LINE,
                         "'%s' is not a function", n->name);
                sem_err(ctx, n->line, msg);
            } else if (n->nchildren > s->nparams) {
                char msg[MAX_LINE];
                snprintf(msg, MAX_LINE,
                         "Too many arguments for '%s': expected %d, got %d",
                         n->name, s->nparams, n->nchildren);
                sem_err(ctx, n->line, msg);
            } else if (n->nchildren < s->nparams) {
                char msg[MAX_LINE];
                snprintf(msg, MAX_LINE,
                         "Too few arguments for '%s': expected %d, got %d",
                         n->name, s->nparams, n->nchildren);
                sem_err(ctx, n->line, msg);
            }
            /* always walk argument expressions, even if arity is wrong */
            for (int i = 0; i < n->nchildren; i++)
                sem_expr(ctx, locals, n->children[i]);
            break;
        }

        case NODE_BINOP:
            sem_expr(ctx, locals, n->children[0]);
            sem_expr(ctx, locals, n->children[1]);
            break;
    }
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
}

/* =========================================================
 *  Statement walker
 * ========================================================= */

static void sem_stmt(SemCtx *ctx, SemTable *locals, const ASTNode *n) {
    if (!n) return;

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"
#endif
    switch (n->type) {

        case NODE_VAR_DECL: {
            if (sem_find(locals, n->name)) {
                char msg[MAX_LINE];
                snprintf(msg, MAX_LINE, "Duplicate declaration of '%s'", n->name);
                sem_err(ctx, n->line, msg);
            } else {
                sem_add(locals, n->name, SEM_VAR, 0);
            }
            if (n->nchildren > 0)
                sem_expr(ctx, locals, n->children[0]);
            break;
        }

        case NODE_ASSIGN: {
            SemSym *s = sem_lookup(ctx, locals, n->name);
            if (!s) {
                char msg[MAX_LINE];
                snprintf(msg, MAX_LINE, "Undefined variable '%s'", n->name);
                sem_err(ctx, n->line, msg);
            } else if (s->kind == SEM_FUNC) {
                char msg[MAX_LINE];
                snprintf(msg, MAX_LINE,
                         "'%s' is a function, not a variable", n->name);
                sem_err(ctx, n->line, msg);
            }
            sem_expr(ctx, locals, n->children[0]);
            break;
        }

        case NODE_IF:
            sem_expr(ctx, locals, n->children[0]);
            sem_block(ctx, locals, n->children[1]);
            if (n->nchildren >= 3)
                sem_block(ctx, locals, n->children[2]);
            break;

        case NODE_WHILE:
            sem_expr(ctx, locals, n->children[0]);
            sem_block(ctx, locals, n->children[1]);
            break;

        case NODE_RETURN:
            sem_expr(ctx, locals, n->children[0]);
            break;

        case NODE_CALL:
            /* reuse the expression walker — handles validation and arg walks */
            sem_expr(ctx, locals, n);
            break;
    }
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
}

static void sem_block(SemCtx *ctx, SemTable *locals, const ASTNode *n) {
    if (!n) return;
    for (int i = 0; i < n->nchildren; i++)
        sem_stmt(ctx, locals, n->children[i]);
}

/* =========================================================
 *  semantic() — entry point
 * ========================================================= */

int semantic(const ASTNode *root, ErrorList *el) {
    SemCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.el = el;

    /* ---- pass 0: register all function names with nparams ---------------
       Done first so forward and mutual calls can be validated.            */
    for (int i = 0; i < root->nchildren; i++) {
        const ASTNode *decl = root->children[i];
        if (decl->type != NODE_FUNC) continue;
        if (sem_find(&ctx.globals, decl->name)) {
            char msg[MAX_LINE];
            snprintf(msg, MAX_LINE, "Duplicate function '%s'", decl->name);
            sem_err(&ctx, decl->line, msg);
        } else {
            sem_add(&ctx.globals, decl->name, SEM_FUNC, decl->nparams);
        }
    }

    /* ---- pass 1: register global variables ------------------------------ */
    for (int i = 0; i < root->nchildren; i++) {
        const ASTNode *decl = root->children[i];
        if (decl->type != NODE_VAR_DECL) continue;
        if (sem_find(&ctx.globals, decl->name)) {
            char msg[MAX_LINE];
            snprintf(msg, MAX_LINE, "Duplicate declaration of '%s'", decl->name);
            sem_err(&ctx, decl->line, msg);
        } else {
            sem_add(&ctx.globals, decl->name, SEM_VAR, 0);
        }
        /* walk global initialiser expressions with no local scope */
        if (decl->nchildren > 0) {
            SemTable empty;
            memset(&empty, 0, sizeof(empty));
            sem_expr(&ctx, &empty, decl->children[0]);
        }
    }

    /* ---- walk each function body ---------------------------------------- */
    for (int i = 0; i < root->nchildren; i++) {
        const ASTNode *decl = root->children[i];
        if (decl->type != NODE_FUNC) continue;

        SemTable locals;
        memset(&locals, 0, sizeof(locals));

        /* register parameters as locals */
        for (int p = 0; p < decl->nparams; p++)
            sem_add(&locals, decl->params[p], SEM_VAR, 0);

        if (decl->nchildren > 0)
            sem_block(&ctx, &locals, decl->children[0]);
    }

    return el->count == 0 ? 1 : 0;
}
