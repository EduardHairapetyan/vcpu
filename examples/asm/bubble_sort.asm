; ══════════════════════════════════════════════════════════
; bubble_sort.asm — Sort 5 numbers in memory
;
; Sorts mem[0..4] = {50, 20, 40, 10, 30} in ascending order.
; Result: mem[0..4] = {10, 20, 30, 40, 50}
;
; Demonstrates: nested loops, memory access, comparison,
;               conditional jumps, and subroutines.
; ══════════════════════════════════════════════════════════

    ; Initialize array in memory
    MOV R0, 50
    STORE R0, 0
    MOV R0, 20
    STORE R0, 1
    MOV R0, 40
    STORE R0, 2
    MOV R0, 10
    STORE R0, 3
    MOV R0, 30
    STORE R0, 4

    ; Bubble sort: 4 passes (n-1)
    MOV R7, 4           ; pass counter

outer:
    CMP R7, R7          ; reset: we'll use R6 as swap flag
    MOV R6, 0           ; swapped = false

    ; Compare and swap adjacent pairs
    ; Since we lack indexed addressing, we unroll the inner loop

    ; --- pair (0,1) ---
    LOAD R0, 0
    LOAD R1, 1
    CMP R0, R1
    JUMP_SIGNED skip01    ; if R0 < R1, already ok
    JUMP_ZERO skip01      ; if equal, skip
    ; swap
    STORE R1, 0
    STORE R0, 1
    MOV R6, 1           ; swapped = true
skip01:

    ; --- pair (1,2) ---
    LOAD R0, 1
    LOAD R1, 2
    CMP R0, R1
    JUMP_SIGNED skip12
    JUMP_ZERO skip12
    STORE R1, 1
    STORE R0, 2
    MOV R6, 1
skip12:

    ; --- pair (2,3) ---
    LOAD R0, 2
    LOAD R1, 3
    CMP R0, R1
    JUMP_SIGNED skip23
    JUMP_ZERO skip23
    STORE R1, 2
    STORE R0, 3
    MOV R6, 1
skip23:

    ; --- pair (3,4) ---
    LOAD R0, 3
    LOAD R1, 4
    CMP R0, R1
    JUMP_SIGNED skip34
    JUMP_ZERO skip34
    STORE R1, 3
    STORE R0, 4
    MOV R6, 1
skip34:

    ; Check if any swaps happened
    MOV R5, 0
    CMP R6, R5
    JUMP_ZERO sorted     ; no swaps → already sorted

    DEC R7               ; one fewer pass needed
    JUMP_NOT_ZERO outer  ; continue if passes remain

sorted:
    END
