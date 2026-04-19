/*
 * emulator.h — Public API for the vCPU emulator (C99)
 */
#ifndef EMULATOR_H
#define EMULATOR_H

#include "vcpu.h"

/* Execute one instruction.  Returns 1 to continue, 0 to halt. */
int execute_one(CPU *cpu, const Program *prog, int debug);

/* Run at most max_steps instructions.  Returns actual step count. */
int run_program(CPU *cpu, const Program *prog, int max_steps, int debug);

#endif /* EMULATOR_H */
