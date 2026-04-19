/*
 * disassembler.h — Public API for the vCPU disassembler (C99)
 */
#ifndef DISASSEMBLER_H
#define DISASSEMBLER_H

#include "vcpu.h"

/* Disassemble one instruction into buf (size bufsz). Returns buf. */
char *disassemble_instr(uint16_t instr, char *buf, size_t bufsz);

/* Print a full annotated listing of prog to stdout. */
void disassemble_listing(const Program *prog);

#endif /* DISASSEMBLER_H */
