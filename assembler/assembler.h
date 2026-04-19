/*
 * assembler.h — Public API for the vCPU assembler (C99)
 */
#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include "vcpu.h"

int assemble(const char *source, Program *out, ErrorList *el);

#endif /* ASSEMBLER_H */
