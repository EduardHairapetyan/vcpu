/*
 * compiler.h — Public API and shared types for the VCL compiler (C99)
 *
 * VCL (Virtual CPU Language) compiles to vCPU assembly.
 * Only type: int (8-bit unsigned, 0-255, matching ISA register width).
 *
 * Calling convention (stack-based):
 *   Arguments   pushed right-to-left by the caller; callee pops them
 *               left-to-right into data-memory slots.  CALL pushes the
 *               return address on top of the args, so the callee prologue
 *               saves it in R7, pops all params, then pushes it back.
 *   Return value in R0.
 *   R0-R7   free for expression evaluation throughout the function body.
 *   SP      managed by PUSH/POP and CALL/RET.
 *
 * Limitations (intentional — match ISA constraints):
 *   No pointers or arrays  (ISA has no register-indirect addressing)
 *   No recursion           (locals are statically allocated)
 *   Max 3 function args    (enforced by MAX_PARAMS / semantic check)
 *   Values wrap at 255     (8-bit unsigned arithmetic)
 */

#ifndef COMPILER_H
#define COMPILER_H

#include "vcpu.h"   /* ErrorList, MAX_ERRORS, MAX_LINE, standard headers */

/* =========================================================
 *  Compiler capacities
 * ========================================================= */

#define MAX_NAME       64   /* max identifier length              */
#define MAX_CHILDREN   64   /* max children per AST node          */
#define MAX_PARAMS      3   /* max function parameters            */
#define MAX_NODES    1024   /* AST node pool size                 */
#define MAX_TOKENS   2048   /* token buffer size                  */
#define MAX_SYMS      128   /* symbol table capacity              */
#define COMP_MEM_LIMIT 96   /* last addressable byte for statics  */

/* =========================================================
 *  Tokens
 * ========================================================= */

typedef enum {
    /* keywords */
    TOK_FUNC, TOK_VAR, TOK_IF, TOK_ELSE, TOK_WHILE, TOK_RETURN,
    /* literals and identifiers */
    TOK_IDENT, TOK_NUMBER,
    /* arithmetic / bitwise operators */
    TOK_PLUS, TOK_MINUS, TOK_AMP, TOK_PIPE, TOK_CARET,
    /* comparison operators */
    TOK_EQ, TOK_NEQ, TOK_LT, TOK_GT, TOK_LEQ, TOK_GEQ,
    /* assignment */
    TOK_ASSIGN,
    /* punctuation */
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE,
    TOK_COMMA, TOK_SEMICOLON,
    /* control */
    TOK_EOF, TOK_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char      text[MAX_NAME];
    int       line;
} Token;

/* =========================================================
 *  AST nodes
 * ========================================================= */

/* Sentinel values for two-char comparison operator ivals */
#define BINOP_EQ   256   /* == */
#define BINOP_NEQ  257   /* != */
#define BINOP_LEQ  258   /* <= */
#define BINOP_GEQ  259   /* >= */

typedef enum {
    NODE_PROGRAM,  /* root; children = top-level declarations            */
    NODE_FUNC,     /* .name .params .nparams; children[0] = body block   */
    NODE_VAR_DECL, /* .name; children[0] = init expr (may be absent)     */
    NODE_BLOCK,    /* children = statements                              */
    NODE_ASSIGN,   /* .name = lhs var; children[0] = rhs expr            */
    NODE_IF,       /* children[0]=cond [1]=then [2]=else (optional)      */
    NODE_WHILE,    /* children[0]=cond [1]=body                          */
    NODE_RETURN,   /* children[0] = return expr                          */
    NODE_CALL,     /* .name = callee; children = arg exprs               */
    NODE_BINOP,    /* .ival = operator char; children[0]=left [1]=right  */
    NODE_IDENT,    /* .name = variable name                              */
    NODE_NUMBER,   /* .ival = integer value 0-255                        */
} NodeType;

typedef struct ASTNode ASTNode;
struct ASTNode {
    NodeType  type;
    int       line;
    char      name[MAX_NAME];
    int       ival;
    char      params[MAX_PARAMS][MAX_NAME];
    int       nparams;
    ASTNode  *children[MAX_CHILDREN];
    int       nchildren;
};

/* =========================================================
 *  Public API
 * ========================================================= */

/* Reset internal AST pool — call before each compile(). */
void     ast_reset(void);

/* Tokenise source text.  Returns token count (excl. EOF) or -1 on error. */
int      lex(const char *source, Token *out, int max_tokens, ErrorList *el);

/* Parse token array produced by lex().  Returns root node or NULL. */
ASTNode *parse(const Token *tokens, ErrorList *el);

/* Walk AST and check for semantic errors (undeclared names, wrong arg counts,
   duplicate declarations).  Returns 1 if clean, 0 if errors were recorded. */
int      semantic(const ASTNode *root, ErrorList *el);

/* Walk AST and write assembly into out_buf (size out_sz).
   Returns 1 on success, 0 on error. */
int      codegen(const ASTNode *root, char *out_buf, int out_sz, ErrorList *el);

/* High-level pipeline: source text → assembly text.
   Returns 1 on success, 0 on error. */
int      compile(const char *source, char *out_buf, int out_sz, ErrorList *el);

#endif /* COMPILER_H */
