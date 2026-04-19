; ══════════════════════════════════════════════════════════
; factorial.asm — Compute 5! = 120 using subroutines
;
; Demonstrates: CALL, RET, PUSH, POP, CMP, loops
; Result: mem[0] = 120
; ══════════════════════════════════════════════════════════

    MOV R0, 5           ; n = 5
    MOV R1, 1           ; result = 1
    CALL factorial
    STORE R1, 0         ; mem[0] = 5! = 120
    END

; ── factorial subroutine ──────────────────────────────────
; Input:  R0 = n, R1 = accumulator (1)
; Output: R1 = n!
; Uses:   R3, R4, R5 as temporaries
factorial:
    MOV R3, 1
    CMP R0, R3          ; if n <= 1 → done
    JUMP_ZERO fact_done
    JUMP_SIGNED fact_done

    ; multiply: R1 = R1 * R0  (repeated addition)
    PUSH R0             ; save n
    PUSH R1             ; save current result
    MOV R4, 0           ; product accumulator
    MOV R5, 0           ; counter

mul_loop:
    ADD R4, R1          ; product += R1
    INC R5
    CMP R5, R0          ; counter == n?
    JUMP_NOT_ZERO mul_loop

    MOV R1, R4          ; R1 = product
    POP R4              ; discard old R1
    POP R0              ; restore n
    DEC R0              ; n--
    JUMP factorial      ; tail-call

fact_done:
    RET
