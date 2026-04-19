# Compiler

The compiler translates VCL (Virtual CPU Language) source files to vCPU assembly. The pipeline is:

```
source.vcl
    ↓  lexer    (tokenise)
 Token[]
    ↓  parser   (recursive descent)
 ASTNode tree
    ↓  codegen  (AST walk)
 output.asm
```

## Usage

```sh
build/bin/compiler <input.vcl> <output.asm>
```

```sh
build/bin/compiler examples/vcl/gcd.vcl /tmp/gcd.asm
# Compiled -> /tmp/gcd.asm
```

The `.asm` output is fed directly to the assembler:

```sh
build/bin/assembler /tmp/gcd.asm /tmp/gcd.bin
build/bin/emulator  /tmp/gcd.bin
```

---

## VCL Language Reference

### Types

VCL has exactly one type: **`int`**, an unsigned 8-bit integer (values 0–255). All arithmetic is modulo 256. There are no pointers, arrays, strings, or structs — these would require register-indirect addressing, which the ISA does not have.

### Keywords

```
func   var   if   else   while   return
```

### Literals

Integer literals are decimal by default; the `0x` prefix selects hexadecimal.

```
42      0xFF      0x0F
```

Values outside 0–255 are rejected at compile time.

### Comments

Line comments begin with `//` and extend to end-of-line. There are no block comments.

```vcl
// this entire line is a comment
var x = 10;   // inline comment
```

### Variables

Variables are declared with `var` and hold one `int` value. All variables are statically allocated to data-memory addresses at compile time — there is no runtime allocation.

```vcl
var x;          // default-initialised to 0
var y = 42;     // initialised to 42
```

**Global variables** are declared at the top level (outside any function). They are initialised once before `main` is called.

**Local variables** are declared inside a function body. Each call to the function reuses the same memory address. This means VCL functions cannot be recursive — a recursive call would overwrite the caller's locals.

### Functions

```vcl
func name(param1, param2) {
    // body
    return expr;
}
```

- Parameter count: 0–3.
- Parameters and return values are `int`.
- A function that falls off the end without an explicit `return` returns 0.
- Calling convention: R0–R2 hold arguments, R0 holds the return value.

### Operators

| Operator | Meaning |
|---|---|
| `+` | addition (wraps at 255) |
| `-` | subtraction (wraps) |
| `&` | bitwise AND |
| `\|` | bitwise OR |
| `^` | bitwise XOR |
| `==` | equal |
| `!=` | not equal |
| `<` | less than (unsigned) |
| `>` | greater than (unsigned) |
| `<=` | less than or equal (unsigned) |
| `>=` | greater than or equal (unsigned) |

There is no multiplication, division, modulo, or shift operator. These can be implemented in VCL using helper functions (see `examples/vcl/factorial.vcl` for a multiply-via-addition helper).

Operator precedence: comparison operators bind less tightly than arithmetic/bitwise. Parentheses can override any precedence:

```vcl
return (a + b) == (c & 0x0F);
```

### Statements

#### Variable declaration

```vcl
var x;            // default zero
var y = expr;     // initialised
```

#### Assignment

```vcl
x = expr;
```

#### If / else

```vcl
if (cond) {
    // ...
}

if (cond) {
    // ...
} else {
    // ...
}
```

Both the then-block and the else-block require braces.

#### While loop

```vcl
while (cond) {
    // ...
}
```

#### Return

```vcl
return expr;
```

#### Function call as statement

```vcl
foo(a, b);   // return value is discarded
```

### Grammar (simplified BNF)

```
program     := (func_decl | var_decl)*
func_decl   := 'func' IDENT '(' param_list? ')' block
param_list  := IDENT (',' IDENT)*
var_decl    := 'var' IDENT ('=' expr)? ';'
block       := '{' stmt* '}'
stmt        := var_decl | assign | if_stmt | while_stmt | return_stmt | call_stmt
assign      := IDENT '=' expr ';'
if_stmt     := 'if' '(' expr ')' block ('else' block)?
while_stmt  := 'while' '(' expr ')' block
return_stmt := 'return' expr ';'
call_stmt   := IDENT '(' arg_list? ')' ';'
expr        := addition (CMP_OP addition)?
addition    := primary (ARITH_OP primary)*
primary     := NUMBER | IDENT | call_expr | '(' expr ')'
call_expr   := IDENT '(' arg_list? ')'
arg_list    := expr (',' expr)*
CMP_OP      := '==' | '!=' | '<' | '>' | '<=' | '>='
ARITH_OP    := '+' | '-' | '&' | '|' | '^'
```

---

## Examples

### Counter (while loop, globals)

```vcl
var result = 0;

func main() {
    var i = 0;
    while (i < 5) {
        result = result + 1;
        i = i + 1;
    }
    return result;   // R0 = 5
}
```

### GCD (nested if/else inside while)

```vcl
func gcd(a, b) {
    while (a != b) {
        if (a > b) {
            a = a - b;
        } else {
            b = b - a;
        }
    }
    return a;
}

func main() {
    return gcd(48, 36);   // R0 = 12
}
```

### Factorial (multiply helper, <= condition)

```vcl
func multiply(a, b) {
    var result = 0;
    var i = 0;
    while (i < b) {
        result = result + a;
        i = i + 1;
    }
    return result;
}

func factorial(n) {
    var result = 1;
    var i = 2;
    while (i <= n) {
        result = multiply(result, i);
        i = i + 1;
    }
    return result;
}

func main() {
    return factorial(5);   // R0 = 120
}
```

---

## Error messages

| Message | Cause |
|---|---|
| `Line N: Unexpected character 'X'` | Invalid character in source |
| `Line N: Too many tokens` | Source has more than 2048 tokens |
| `Line N: Integer literal N out of range 0-255` | Numeric literal exceeds 255 |
| `Line N: Expected X but got 'Y'` | Syntax error |
| `Line N: Function has more than 3 parameters` | Exceeds ISA calling convention |
| `Line N: Unexpected token 'X' in expression` | Invalid expression start |
| `Line N: Undefined variable 'X'` | Variable used before declaration |
| `Line N: Undefined function 'X'` | Call to a function that was never declared |
| `Line N: 'X' is a function, not a variable` | Function name used where a variable value is expected |
| `Line N: Too many arguments (max 3)` | Call with more than 3 arguments |

---

## Compiler internals

### Calling convention

| Register | Role |
|---|---|
| R0 | Argument 1 / return value |
| R1 | Argument 2 |
| R2 | Argument 3 |
| R3–R5 | Expression scratch (caller does not preserve across calls) |
| SP | Stack pointer (managed by PUSH/POP/CALL/RET) |

### Expression evaluation strategy

All expressions evaluate into R0. Binary operations use a stack-based strategy: the right operand is evaluated into R0 and pushed, then the left operand is evaluated into R0, then the right operand is popped into R1, and the operation is applied.

```vcl
a + b
```

Generates:

```asm
LOAD R0, <addr_b>
PUSH R0
LOAD R0, <addr_a>
POP  R1
ADD  R0, R1          ; result in R0
```

This is correct for arbitrarily deep expression trees because intermediate values are safely on the stack while sub-expressions are evaluated.

### Function calls

Arguments are pushed right-to-left, then popped into R0, R1, R2 in order:

```vcl
foo(a, b, c)
```

Generates:

```asm
LOAD R0, <addr_c>
PUSH R0
LOAD R0, <addr_b>
PUSH R0
LOAD R0, <addr_a>
PUSH R0
POP  R0
POP  R1
POP  R2
CALL foo
```

### Comparison codegen

Comparisons used as **conditions** in `if`/`while` never materialise a 0/1 value. Instead, CMP is emitted directly followed by the appropriate conditional jump.

Comparisons used as **values** in expressions (e.g. `return a == b`) materialise: a forward jump to a "true" label emits `MOV R0, 1`, the false path emits `MOV R0, 0`.

### Memory layout

Variables are assigned data-memory addresses in the order they are declared, starting from address 0:

```
mem[0]  → first global
mem[1]  → second global
...
mem[N]  → first local of first function
mem[N+1]→ first parameter of first function
...
```

The compiler reserves the upper region of data memory for the stack. `COMP_MEM_LIMIT` (defined as 96) is the last address the allocator will assign, leaving addresses 96–255 free for stack growth.

### Source files

| File | Responsibility |
|---|---|
| `src/compiler.h` | Token enum, ASTNode struct, ErrorList alias, public API |
| `src/lexer.c` | Source text → flat `Token` array |
| `src/parser.c` | Token array → AST; includes the static node pool |
| `src/codegen.c` | AST → assembly text; symbol table, label counter |
| `src/compiler.c` | `main()` driver; reads file, calls lexer→parser→codegen |

---

## Limitations

These are intentional constraints that match the ISA:

- **No pointers or arrays** — the ISA has no register-indirect addressing mode, so there is no way to compute a runtime memory address.
- **No recursion** — locals are statically allocated per function. A recursive call would overwrite the current frame.
- **Maximum 3 function arguments** — only R0–R2 are available as argument registers.
- **No multiplication, division, or shift operators** — not in the ISA; implement using helper functions.
- **Values wrap at 255** — honest 8-bit unsigned semantics.
