; ══════════════════════════════════════════════════════════
; fibonacci.asm — Compute first 13 Fibonacci numbers
;
; Since the ISA lacks indexed addressing, we unroll the
; stores. This is a realistic limitation of the architecture.
;
; Result: mem[0..12] = 0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144
; ══════════════════════════════════════════════════════════

    MOV R0, 0           ; fib(n-2) = 0
    MOV R1, 1           ; fib(n-1) = 1

    ; Store first two
    STORE R0, 0
    STORE R1, 1

    ; Macro pattern: R2 = R1+R0, store, shift
    MOV R2, R1
    ADD R2, R0
    STORE R2, 2
    MOV R0, R1
    MOV R1, R2

    MOV R2, R1
    ADD R2, R0
    STORE R2, 3
    MOV R0, R1
    MOV R1, R2

    MOV R2, R1
    ADD R2, R0
    STORE R2, 4
    MOV R0, R1
    MOV R1, R2

    MOV R2, R1
    ADD R2, R0
    STORE R2, 5
    MOV R0, R1
    MOV R1, R2

    MOV R2, R1
    ADD R2, R0
    STORE R2, 6
    MOV R0, R1
    MOV R1, R2

    MOV R2, R1
    ADD R2, R0
    STORE R2, 7
    MOV R0, R1
    MOV R1, R2

    MOV R2, R1
    ADD R2, R0
    STORE R2, 8
    MOV R0, R1
    MOV R1, R2

    MOV R2, R1
    ADD R2, R0
    STORE R2, 9
    MOV R0, R1
    MOV R1, R2

    MOV R2, R1
    ADD R2, R0
    STORE R2, 10
    MOV R0, R1
    MOV R1, R2

    MOV R2, R1
    ADD R2, R0
    STORE R2, 11
    MOV R0, R1
    MOV R1, R2

    MOV R2, R1
    ADD R2, R0
    STORE R2, 12

    END
