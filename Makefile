# =============================================================================
# vCPU Toolchain — Unified Makefile (C99)
#
# Project layout:
#   include/        vcpu.h            shared ISA definitions
#   assembler/      assembler.h/.c    two-pass assembler
#   disassembler/   disassembler.h/.c listing generator
#   emulator/       emulator.h/.c     instruction interpreter
#   compiler/       compiler.h/.c     VCL compiler (+ lexer, parser, codegen)
#   tests/          test_*.c          all unit and integration tests
#   examples/asm/   *.asm             hand-written assembly programs
#   examples/vcl/   *.vcl             VCL high-level programs
#   build/bin/      binaries          compiled tools and test runners
#   build/ex/asm/   *.bin             assembled ASM examples
#   build/ex/vcl/   *.asm *.bin       compiled+assembled VCL examples
#
# Primary targets:
#   make            — build all four tools
#   make test       — build and run all tests, print summary
#   make examples   — assemble / compile and run all example programs
#   make clean      — remove build/
# =============================================================================

CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -Wpedantic

# -I flags: every compilation can see all component directories and tests/
INC     = -I include -I assembler -I disassembler -I emulator -I compiler -I tests

# ---- output directories -----------------------------------------------------
BINDIR  = build/bin
EXADIR  = build/ex/asm
EXVDIR  = build/ex/vcl

# ---- tool binaries ----------------------------------------------------------
ASSEMBLER    = $(BINDIR)/assembler
DISASSEMBLER = $(BINDIR)/disassembler
EMULATOR     = $(BINDIR)/emulator
COMPILER     = $(BINDIR)/compiler

# ---- test binaries ----------------------------------------------------------
TEST_ASM   = $(BINDIR)/test_assembler
TEST_DIS   = $(BINDIR)/test_disassembler
TEST_EMU   = $(BINDIR)/test_emulator
TEST_INT   = $(BINDIR)/test_integration
TEST_LEX   = $(BINDIR)/test_lexer
TEST_PAR   = $(BINDIR)/test_parser
TEST_SEM   = $(BINDIR)/test_semantic
TEST_COD   = $(BINDIR)/test_codegen
TEST_CALL  = $(BINDIR)/test_call_preservation

TESTS = $(TEST_ASM) $(TEST_DIS) $(TEST_EMU) $(TEST_INT) \
        $(TEST_LEX) $(TEST_PAR) $(TEST_SEM) $(TEST_COD) $(TEST_CALL)

# ---- example source lists ---------------------------------------------------
ASM_SRCS := $(wildcard examples/asm/*.asm)
VCL_SRCS := $(wildcard examples/vcl/*.vcl)

ASM_BINS := $(patsubst examples/asm/%.asm,$(EXADIR)/%.bin,$(ASM_SRCS))
VCL_ASMS := $(patsubst examples/vcl/%.vcl,$(EXVDIR)/%.asm,$(VCL_SRCS))
VCL_BINS := $(patsubst examples/vcl/%.vcl,$(EXVDIR)/%.bin,$(VCL_SRCS))

# =============================================================================
.PHONY: all test examples clean help

# =============================================================================
#  all — build all four tools
# =============================================================================
all: $(ASSEMBLER) $(DISASSEMBLER) $(EMULATOR) $(COMPILER)

# ---- directory rule ---------------------------------------------------------
$(BINDIR) $(EXADIR) $(EXVDIR):
	mkdir -p $@

# ---- assembler --------------------------------------------------------------
$(ASSEMBLER): assembler/assembler.c assembler/assembler.h include/vcpu.h | $(BINDIR)
	$(CC) $(CFLAGS) $(INC) -o $@ assembler/assembler.c

# ---- disassembler -----------------------------------------------------------
$(DISASSEMBLER): disassembler/disassembler.c include/vcpu.h | $(BINDIR)
	$(CC) $(CFLAGS) $(INC) -o $@ disassembler/disassembler.c

# ---- emulator ---------------------------------------------------------------
$(EMULATOR): emulator/emulator.c include/vcpu.h | $(BINDIR)
	$(CC) $(CFLAGS) $(INC) -o $@ emulator/emulator.c

# ---- compiler ---------------------------------------------------------------
$(COMPILER): compiler/compiler.c compiler/compiler.h compiler/lexer.c \
             compiler/parser.c compiler/semantic.c compiler/codegen.c | $(BINDIR)
	$(CC) $(CFLAGS) $(INC) -o $@ compiler/compiler.c

# =============================================================================
#  test — build and run all test suites, print a pass/fail summary
# =============================================================================

# assembler tests
$(TEST_ASM): tests/test_assembler.c assembler/assembler.h assembler/assembler.c \
             include/vcpu.h tests/test_framework.h | $(BINDIR)
	$(CC) $(CFLAGS) $(INC) -DVCPU_TESTING -o $@ tests/test_assembler.c

# disassembler tests
$(TEST_DIS): tests/test_disassembler.c disassembler/disassembler.c include/vcpu.h \
             tests/test_framework.h | $(BINDIR)
	$(CC) $(CFLAGS) $(INC) -DVCPU_TESTING -o $@ tests/test_disassembler.c

# emulator tests
$(TEST_EMU): tests/test_emulator.c emulator/emulator.c assembler/assembler.c \
             assembler/assembler.h include/vcpu.h tests/test_framework.h | $(BINDIR)
	$(CC) $(CFLAGS) $(INC) -DVCPU_TESTING -o $@ tests/test_emulator.c

# integration tests
$(TEST_INT): tests/test_integration.c assembler/assembler.c assembler/assembler.h \
             emulator/emulator.c include/vcpu.h tests/test_framework.h | $(BINDIR)
	$(CC) $(CFLAGS) $(INC) -DVCPU_TESTING -o $@ tests/test_integration.c

# lexer tests
$(TEST_LEX): tests/test_lexer.c compiler/compiler.h compiler/lexer.c \
             tests/test_framework.h | $(BINDIR)
	$(CC) $(CFLAGS) $(INC) -DVCPU_TESTING -o $@ tests/test_lexer.c

# parser tests
$(TEST_PAR): tests/test_parser.c compiler/compiler.h compiler/lexer.c \
             compiler/parser.c tests/test_framework.h | $(BINDIR)
	$(CC) $(CFLAGS) $(INC) -DVCPU_TESTING -o $@ tests/test_parser.c

# semantic analysis tests
$(TEST_SEM): tests/test_semantic.c compiler/compiler.h compiler/lexer.c \
             compiler/parser.c compiler/semantic.c \
             include/vcpu.h tests/test_framework.h | $(BINDIR)
	$(CC) $(CFLAGS) $(INC) -DVCPU_TESTING -o $@ tests/test_semantic.c

# codegen / end-to-end compiler tests
$(TEST_COD): tests/test_codegen.c compiler/compiler.h compiler/lexer.c \
             compiler/parser.c compiler/semantic.c compiler/codegen.c \
             assembler/assembler.c assembler/assembler.h emulator/emulator.c \
             include/vcpu.h tests/test_framework.h | $(BINDIR)
	$(CC) $(CFLAGS) $(INC) -DVCPU_TESTING -o $@ tests/test_codegen.c

# register preservation across CALL/RET tests
$(TEST_CALL): tests/test_call_preservation.c assembler/assembler.c \
              assembler/assembler.h emulator/emulator.c \
              include/vcpu.h tests/test_framework.h | $(BINDIR)
	$(CC) $(CFLAGS) $(INC) -DVCPU_TESTING -o $@ tests/test_call_preservation.c

test: $(TESTS)
	@total=0; failed=0; \
	for t in $(TESTS); do \
	    name=$$(basename $$t); \
	    result=$$(./$$t 2>&1); \
	    line=$$(echo "$$result" | grep "test(s) run"); \
	    n=$$(echo "$$line" | awk '{print $$1}'); \
	    f=$$(echo "$$line" | awk '{print $$4}'); \
	    total=$$((total + n)); failed=$$((failed + f)); \
	    if [ "$$f" = "0" ]; then \
	        printf "  %-22s %3d tests   \033[32mPASS\033[0m\n" "$$name" "$$n"; \
	    else \
	        printf "  %-22s %3d tests   \033[31mFAIL  ($$f failed)\033[0m\n" "$$name" "$$n"; \
	        echo "$$result" | grep "FAIL" | sed 's/^/    /'; \
	    fi; \
	done; \
	echo ""; \
	echo "════════════════════════════════════════════════"; \
	if [ $$failed -eq 0 ]; then \
	    printf "  \033[32m$$total tests run — all passed\033[0m\n"; \
	else \
	    printf "  \033[31m$$total tests run — $$failed FAILED\033[0m\n"; \
	fi; \
	echo "════════════════════════════════════════════════"; \
	[ $$failed -eq 0 ]

# =============================================================================
#  examples — assemble/compile every example and run it through the emulator
# =============================================================================

# ASM examples: assemble → run
$(EXADIR)/%.bin: examples/asm/%.asm $(ASSEMBLER) | $(EXADIR)
	@$(ASSEMBLER) $< $@ 2>/dev/null

# VCL examples: compile → assemble → run
$(EXVDIR)/%.asm: examples/vcl/%.vcl $(COMPILER) | $(EXVDIR)
	@$(COMPILER) $< $@ 2>/dev/null

$(EXVDIR)/%.bin: $(EXVDIR)/%.asm $(ASSEMBLER)
	@$(ASSEMBLER) $< $@ 2>/dev/null

# Keep intermediate VCL .asm files so the user can inspect generated assembly
.PRECIOUS: $(VCL_ASMS)

examples: all $(ASM_BINS) $(VCL_BINS)
	@echo ""
	@echo "════════════════════════════════════════════════"
	@echo "  Assembly examples"
	@echo "════════════════════════════════════════════════"
	@for bin in $(ASM_BINS); do \
	    name=$$(basename $$bin .bin); \
	    r0=$$($(EMULATOR) $$bin 2>/dev/null | grep "R0 =" | awk '{print $$4, $$5}'); \
	    printf "  %-18s R0 = %s\n" "$$name" "$$r0"; \
	done
	@echo ""
	@echo "════════════════════════════════════════════════"
	@echo "  VCL examples"
	@echo "════════════════════════════════════════════════"
	@for bin in $(VCL_BINS); do \
	    name=$$(basename $$bin .bin); \
	    r0=$$($(EMULATOR) $$bin 2>/dev/null | grep "R0 =" | awk '{print $$4, $$5}'); \
	    printf "  %-18s R0 = %s\n" "$$name" "$$r0"; \
	done
	@echo ""

# =============================================================================
#  clean
# =============================================================================
clean:
	rm -rf build

# =============================================================================
#  help
# =============================================================================
help:
	@echo "vCPU Toolchain — targets:"
	@echo ""
	@echo "  make              Build all tools (assembler, disassembler,"
	@echo "                    emulator, compiler)"
	@echo "  make test         Build and run all 7 test suites"
	@echo "  make examples     Compile/assemble and run all examples"
	@echo "  make clean        Remove build/"
	@echo ""
	@echo "  Tool binaries after 'make all':"
	@echo "    build/bin/assembler     <in.asm>  <out.bin>"
	@echo "    build/bin/disassembler  <prog.bin>"
	@echo "    build/bin/emulator      <prog.bin>  [--debug]"
	@echo "    build/bin/compiler      <in.vcl>  <out.asm>"
	@echo ""
	@echo "  Full pipeline (VCL source → running program):"
	@echo "    build/bin/compiler   prog.vcl  prog.asm"
	@echo "    build/bin/assembler  prog.asm  prog.bin"
	@echo "    build/bin/emulator   prog.bin"
