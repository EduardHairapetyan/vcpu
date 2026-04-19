; ══════════════════════════════════════════════════════════
; r7_test.asm — Verify R7 works as ALU source operand
;
; This test was created to verify the fix for the R7/sentinel
; conflict that existed in v2 of the ISA. All 8 registers
; must work correctly as both source and destination.
;
; Expected results:
;   mem[0] = 150  (ADD R0, R7:  100 + 50)
;   mem[1] = 50   (SUB R1, R7:  100 - 50)
;   mem[2] = 32   (AND R2, R7:  0xFF & 0x20 = 0x20 = 32)
;   mem[3] = 255  (OR  R3, R7:  0xCF | 0x30 = 0xFF = 255)
;   mem[4] = 255  (XOR R4, R7:  0xAA ^ 0x55 = 0xFF = 255)
; ══════════════════════════════════════════════════════════

    ; Test ADD with R7
    MOV R0, 100
    MOV R7, 50
    ADD R0, R7
    STORE R0, 0         ; expect 150

    ; Test SUB with R7
    MOV R1, 100
    MOV R7, 50
    SUB R1, R7
    STORE R1, 1         ; expect 50

    ; Test AND with R7
    MOV R2, 0xFF
    MOV R7, 0x20
    AND R2, R7
    STORE R2, 2         ; expect 32

    ; Test OR with R7
    MOV R3, 0xCF
    MOV R7, 0x30
    OR R3, R7
    STORE R3, 3         ; expect 255

    ; Test XOR with R7
    MOV R4, 0xAA
    MOV R7, 0x55
    XOR R4, R7
    STORE R4, 4         ; expect 255

    END
