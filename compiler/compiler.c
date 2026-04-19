/*
 * compiler.c — CLI driver for the VCL compiler (C99)
 *
 * Usage: ./compiler <input.vcl> <output.asm>
 */

#include "compiler.h"
#include <ctype.h>

#include "lexer.c"
#include "parser.c"
#include "semantic.c"
#include "codegen.c"

/* 64 KB output buffer — plenty for programs targeting 256 instructions */
#define OUT_BUF_SZ (64 * 1024)

#ifndef VCPU_TESTING

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: compiler <input.vcl> <output.asm>\n");
        return 1;
    }

    FILE *fin = fopen(argv[1], "r");
    if (!fin) { fprintf(stderr, "Cannot open: %s\n", argv[1]); return 1; }

    fseek(fin, 0, SEEK_END);
    long fsz = ftell(fin);
    if (fsz < 0) {
        fprintf(stderr, "Error: cannot determine size of '%s'\n", argv[1]);
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

    char *out_buf = (char *)malloc(OUT_BUF_SZ);
    if (!out_buf) { free(source); return 1; }

    ErrorList el;
    el.count = 0;

    int ok = compile(source, out_buf, OUT_BUF_SZ, &el);
    free(source);

    if (!ok || el.count > 0) {
        for (int i = 0; i < el.count; i++)
            fprintf(stderr, "%s\n", el.msgs[i]);
        free(out_buf);
        return 1;
    }

    FILE *fout = fopen(argv[2], "w");
    if (!fout) {
        fprintf(stderr, "Cannot write: %s\n", argv[2]);
        free(out_buf);
        return 1;
    }
    fputs(out_buf, fout);
    fclose(fout);
    free(out_buf);

    printf("Compiled -> %s\n", argv[2]);
    return 0;
}

#endif /* VCPU_TESTING */
