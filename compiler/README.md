# compiler/

Compiles VCL (Virtual CPU Language) source to vCPU assembly. Implemented as a unity build: `compiler.c` includes the three stage files directly.

## Files

| File | Stage | Description |
|---|---|---|
| `compiler.h` | — | Public API, token types, AST node types, compiler capacities |
| `compiler.c` | Driver | CLI entry point; includes lexer, parser, codegen via `#include` |
| `lexer.c` | 1 — Tokenise | Source text → `Token[]` |
| `parser.c` | 2 — Parse | `Token[]` → `ASTNode` tree (recursive-descent) |
| `codegen.c` | 3 — Generate | `ASTNode` tree → vCPU assembly text |

## Public API

```c
#include "compiler.h"

void     ast_reset(void);
int      lex    (const char *source, Token *out, int max_tokens, ErrorList *el);
ASTNode *parse  (const Token *tokens, ErrorList *el);
int      codegen(const ASTNode *root, char *out_buf, int out_sz, ErrorList *el);

/* High-level pipeline: source text → assembly text. */
int      compile(const char *source, char *out_buf, int out_sz, ErrorList *el);
```

## CLI usage

```sh
build/bin/compiler <input.vcl> <output.asm>
```

## VCL language summary

- Only type: `int` (8-bit unsigned, 0–255)
- Constructs: `func`, `var`, `if/else`, `while`, `return`
- Operators: `+  -  &  |  ^  ==  !=  <  >  <=  >=`
- Max 3 function arguments (ISA register constraint)
- No recursion, no pointers (intentional ISA limitations)

See [`../docs/compiler.md`](../docs/compiler.md) for the full grammar and calling convention.
