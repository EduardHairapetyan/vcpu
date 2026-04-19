/*
 * assembler.c — Two-pass assembler for the Virtual CPU (C99)
 *
 * Pass 1: tokenise source lines, collect label addresses
 * Pass 2: encode each instruction to 16-bit binary
 *
 * Usage: ./assembler <input.asm> <output.bin>
 */

#include "assembler.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =========================================================
 *  Constants and macros
 * ========================================================= */

/* MAX_LABELS: 256 matches the maximum number of instruction slots, so you cannot
   have more unique jump targets than addressable slots. */
#define MAX_LABELS  256
#define MAX_ARGS    4
/* MAX_LINES: 16-bit ISA supports 256 instruction slots; 512 gives 2x headroom for
   source lines that include labels and blank lines without producing instructions. */
#define MAX_LINES   512

/* Used inside encode() to report an error and bail out immediately.
   Requires local variables: el (ErrorList *) and L (const Line *). */
#define ERRMN(msg) do { err_add(el, L->lineno, msg); return 0; } while(0)

/* =========================================================
 *  Types
 * ========================================================= */

typedef struct {
    char name[MAX_LINE];
    int  addr;
} Label;

typedef struct {
    Label entries[MAX_LABELS];
    int   count;
} LabelTable;

typedef struct {
    int  lineno;
    char label[MAX_LINE];
    char mnemonic[32];
    char args[MAX_ARGS][MAX_LINE];
    int  nargs;
} Line;

typedef struct {
    Line data[MAX_LINES];
    int  count;
} LineList;

/* =========================================================
 *  Error helper
 * ========================================================= */

static void err_add(ErrorList *el, int lineno, const char *msg) {
    if (el->count >= MAX_ERRORS) return;
    snprintf(el->msgs[el->count], MAX_LINE, "Line %d: %s", lineno, msg);
    el->count++;
}

/* =========================================================
 *  String helpers
 * ========================================================= */

static char *str_trim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)*(end-1))) end--;
    *end = '\0';
    return s;
}

static void str_upper(char *s) {
    for (; *s; s++) *s = (char)toupper((unsigned char)*s);
}

static int parse_register(const char *s) {
    char buf[8];
    strncpy(buf, s, 7); buf[7] = '\0';
    str_upper(buf);
    char *t = str_trim(buf);
    if (t[0] == 'R' && t[1] >= '0' && t[1] <= '7' && t[2] == '\0')
        return t[1] - '0';
    return -1;
}

static int parse_immediate(const char *s, int *out) {
    char *end;
    long v = strtol(s, &end, 0);   /* supports 0x hex, 0b binary */
    while (*end && isspace((unsigned char)*end)) end++;
    if (*end != '\0') return 0;
    *out = (int)v;
    return 1;
}

/* =========================================================
 *  Label table
 * ========================================================= */

static int label_find(const LabelTable *lt, const char *name) {
    for (int i = 0; i < lt->count; i++)
        if (strcmp(lt->entries[i].name, name) == 0)
            return lt->entries[i].addr;
    return -1;
}

static int label_add(LabelTable *lt, const char *name, int addr, ErrorList *el, int lineno) {
    if (label_find(lt, name) >= 0) {
        char msg[MAX_LINE];
        snprintf(msg, MAX_LINE, "Duplicate label '%s'", name);
        err_add(el, lineno, msg);
        return 0;
    }
    if (lt->count >= MAX_LABELS) {
        err_add(el, lineno, "Too many labels");
        return 0;
    }
    strncpy(lt->entries[lt->count].name, name, MAX_LINE - 1);
    lt->entries[lt->count].addr = addr;
    lt->count++;
    return 1;
}

/* =========================================================
 *  Tokeniser
 * ========================================================= */

static int tokenise(const char *raw, int lineno, Line *out) {
    memset(out, 0, sizeof(*out));
    out->lineno = lineno;

    /* work in a mutable copy */
    char buf[MAX_LINE];
    strncpy(buf, raw, MAX_LINE - 1);
    buf[MAX_LINE - 1] = '\0';

    /* strip comments */
    char *semi = strchr(buf, ';');
    if (semi) *semi = '\0';

    char *s = str_trim(buf);
    if (*s == '\0') return 0;   /* blank line */

    /* label? */
    char *colon = strchr(s, ':');
    if (colon) {
        *colon = '\0';
        char *lbl = str_trim(s);
        strncpy(out->label, lbl, MAX_LINE - 1);
        s = str_trim(colon + 1);
        if (*s == '\0') return 1;   /* label-only line */
    }

    /* mnemonic */
    char *sp = s;
    while (*sp && !isspace((unsigned char)*sp)) sp++;
    int mnlen = (int)(sp - s);
    if (mnlen >= 32) mnlen = 31;
    strncpy(out->mnemonic, s, (size_t)mnlen);
    out->mnemonic[mnlen] = '\0';
    str_upper(out->mnemonic);

    s = str_trim(sp);
    if (*s == '\0') return 1;

    /* arguments: split by comma */
    char *tok = s;
    while (out->nargs < MAX_ARGS) {
        char *comma = strchr(tok, ',');
        if (comma) *comma = '\0';
        char *arg = str_trim(tok);
        if (*arg) {
            strncpy(out->args[out->nargs], arg, MAX_LINE - 1);
            out->nargs++;
        }
        if (!comma) break;
        tok = comma + 1;
    }
    return 1;
}

/* =========================================================
 *  Resolve label or immediate to address (0-255)
 * ========================================================= */

static int resolve_addr(const char *s, const LabelTable *lt,
                        int *out, ErrorList *el, int lineno) {
    int addr = label_find(lt, s);
    if (addr >= 0) { *out = addr; return 1; }
    if (!parse_immediate(s, out)) {
        char msg[MAX_LINE];
        snprintf(msg, MAX_LINE, "Unknown label or bad address: %s", s);
        err_add(el, lineno, msg);
        return 0;
    }
    if (*out < 0 || *out > 255) {
        char msg[MAX_LINE];
        snprintf(msg, MAX_LINE, "Address out of range 0-255: %s", s);
        err_add(el, lineno, msg);
        return 0;
    }
    return 1;
}

/* =========================================================
 *  Encode one instruction
 * ========================================================= */

static int encode(const Line *L, const LabelTable *lt,
                  uint16_t *word, ErrorList *el)
{
    const char *mn = L->mnemonic;

    /* Z-type */
    if (strcmp(mn,"NOP")==0||strcmp(mn,"END")==0||strcmp(mn,"RET")==0) {
        if (L->nargs != 0) ERRMN("no operands expected");
        int op = (strcmp(mn,"NOP")==0) ? OP_NOP :
                 (strcmp(mn,"END")==0) ? OP_END : OP_RET;
        *word = buildZ(op);
        return 1;
    }

    /* S-type */
    if (strcmp(mn,"PUSH")==0||strcmp(mn,"POP")==0||
        strcmp(mn,"INC")==0 ||strcmp(mn,"DEC")==0) {
        if (L->nargs != 1) ERRMN("requires 1 register");
        int r = parse_register(L->args[0]);
        if (r < 0) ERRMN("invalid register");
        int op = (strcmp(mn,"PUSH")==0) ? OP_PUSH :
                 (strcmp(mn,"POP") ==0) ? OP_POP  :
                 (strcmp(mn,"INC") ==0) ? OP_INC  : OP_DEC;
        *word = buildS(op, r);
        return 1;
    }

    /* R-type ALU */
    if (strcmp(mn,"ADD")==0||strcmp(mn,"SUB")==0||strcmp(mn,"AND")==0||
        strcmp(mn,"OR") ==0||strcmp(mn,"XOR")==0||strcmp(mn,"CMP")==0) {
        if (L->nargs != 2) ERRMN("requires 2 registers");
        int rd = parse_register(L->args[0]);
        int rs = parse_register(L->args[1]);
        if (rd < 0) ERRMN("invalid destination register");
        if (rs < 0) ERRMN("invalid source register");
        int op = (strcmp(mn,"ADD")==0) ? OP_ADD :
                 (strcmp(mn,"SUB")==0) ? OP_SUB :
                 (strcmp(mn,"AND")==0) ? OP_AND :
                 (strcmp(mn,"OR") ==0) ? OP_OR  :
                 (strcmp(mn,"XOR")==0) ? OP_XOR : OP_CMP;
        *word = buildR(op, rd, rs);
        return 1;
    }

    /* MOV: R-type (reg,reg) or I-type (reg,imm) */
    if (strcmp(mn, "MOV") == 0) {
        if (L->nargs != 2) ERRMN("MOV requires 2 operands");
        int rd = parse_register(L->args[0]);
        if (rd < 0) ERRMN("invalid destination register");
        int rs = parse_register(L->args[1]);
        if (rs >= 0) { *word = buildR(OP_MOV_REG, rd, rs); return 1; }
        int imm = 0;
        if (!parse_immediate(L->args[1], &imm)) ERRMN("invalid immediate");
        if (imm < 0 || imm > 255) ERRMN("immediate out of range 0-255");
        *word = buildI(OP_MOV_IMM, rd, imm);
        return 1;
    }

    /* LOAD / STORE */
    if (strcmp(mn, "LOAD") == 0) {
        if (L->nargs != 2) ERRMN("LOAD requires 2 operands");
        int rd = parse_register(L->args[0]);
        if (rd < 0) ERRMN("invalid register");
        int addr = 0;
        if (!resolve_addr(L->args[1], lt, &addr, el, L->lineno)) return 0;
        *word = buildI(OP_LOAD, rd, addr);
        return 1;
    }
    if (strcmp(mn, "STORE") == 0) {
        if (L->nargs != 2) ERRMN("STORE requires 2 operands");
        int rs = parse_register(L->args[0]);
        if (rs < 0) ERRMN("invalid register");
        int addr = 0;
        if (!resolve_addr(L->args[1], lt, &addr, el, L->lineno)) return 0;
        *word = buildI(OP_STORE, rs, addr);
        return 1;
    }

    /* J-type: all jumps and CALL */
    {
        int jop = -1;
        if (strcmp(mn,"JUMP")           ==0) jop = OP_JUMP;
        if (strcmp(mn,"JUMP_ZERO")      ==0) jop = OP_JUMP_ZERO;
        if (strcmp(mn,"JUMP_NOT_ZERO")  ==0) jop = OP_JUMP_NOT_ZERO;
        if (strcmp(mn,"JUMP_SIGNED")    ==0) jop = OP_JUMP_SIGNED;
        if (strcmp(mn,"JUMP_NOT_SIGNED")==0) jop = OP_JUMP_NOT_SIGNED;
        if (strcmp(mn,"JUMP_CARRY")     ==0) jop = OP_JUMP_CARRY;
        if (strcmp(mn,"JUMP_NOT_CARRY") ==0) jop = OP_JUMP_NOT_CARRY;
        if (strcmp(mn,"CALL")           ==0) jop = OP_CALL;
        if (jop >= 0) {
            if (L->nargs != 1) ERRMN("jump/call requires 1 operand");
            int addr = 0;
            if (!resolve_addr(L->args[0], lt, &addr, el, L->lineno)) return 0;
            *word = buildJ(jop, addr);
            return 1;
        }
    }

    {
        char msg[MAX_LINE];
        snprintf(msg, MAX_LINE, "Unknown mnemonic: %s", mn);
        err_add(el, L->lineno, msg);
        return 0;
    }
}

/* =========================================================
 *  Public API
 * ========================================================= */

int assemble(const char *source, Program *out, ErrorList *el) {
    LineList  lines;
    LabelTable lt;
    lines.count = 0;
    lt.count    = 0;
    prog_init(out);

    const char *p    = source;
    int         lineno   = 0;
    int         instrAddr = 0;

    /* pass 1: tokenise + collect labels */
    while (*p) {
        const char *eol = strchr(p, '\n');
        char rawbuf[MAX_LINE];
        size_t len = eol ? (size_t)(eol - p) : strlen(p);
        if (len >= MAX_LINE) len = MAX_LINE - 1;
        strncpy(rawbuf, p, len);
        rawbuf[len] = '\0';
        p = eol ? eol + 1 : p + len;
        lineno++;

        if (lines.count >= MAX_LINES) {
            err_add(el, lineno, "Too many lines (max 512)");
            return 0;
        }

        Line L;
        if (!tokenise(rawbuf, lineno, &L)) continue;
        if (L.label[0]) {
            if (!label_add(&lt, L.label, instrAddr, el, lineno)) return 0;
        }
        if (L.mnemonic[0]) {
            lines.data[lines.count++] = L;
            instrAddr++;
        }
    }

    /* pass 2: encode */
    for (int i = 0; i < lines.count; i++) {
        uint16_t word = 0;
        if (encode(&lines.data[i], &lt, &word, el)) {
            if (!prog_push(out, word)) {
                err_add(el, lines.data[i].lineno, "Out of memory while encoding");
                return 0;
            }
        }
    }
    return el->count == 0;
}

/* =========================================================
 *  Main
 * ========================================================= */

#ifndef VCPU_TESTING
int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: assembler <input.asm> <output.bin>\n");
        return 1;
    }

    FILE *fin = fopen(argv[1], "r");
    if (!fin) { fprintf(stderr, "Cannot open: %s\n", argv[1]); return 1; }
    fseek(fin, 0, SEEK_END);
    long fsz = ftell(fin);
    if (fsz < 0) {
        fprintf(stderr, "Error: cannot determine size of '%s' (is it a regular file?)\n", argv[1]);
        fclose(fin);
        return 1;
    }
    rewind(fin);
    char *source = (char *)malloc((size_t)(fsz + 1));
    if (!source) { fclose(fin); return 1; }
    size_t nread = fread(source, 1, (size_t)fsz, fin);
    fclose(fin);
    if (nread != (size_t)fsz) {
        fprintf(stderr, "Error: could not read full file '%s'\n", argv[1]);
        free(source);
        return 1;
    }
    source[fsz] = '\0';

    Program prog;
    ErrorList el;
    el.count = 0;

    assemble(source, &prog, &el);
    free(source);

    if (el.count > 0) {
        for (int i = 0; i < el.count; i++)
            fprintf(stderr, "%s\n", el.msgs[i]);
        prog_free(&prog);
        return 1;
    }

    if (!saveBinary(argv[2], &prog)) {
        prog_free(&prog);
        return 1;
    }

    printf("Assembled %d instruction(s) -> %s\n", prog.size, argv[2]);
    prog_free(&prog);
    return 0;
}
#endif /* VCPU_TESTING */
