; ══════════════════════════════════════════════════════════
; hello.asm — "Hello World" for the Virtual CPU
;
; Since we have no I/O, this stores the ASCII codes for
; "HELLO" in consecutive memory locations (mem[0]-mem[4]).
; ══════════════════════════════════════════════════════════

    MOV R0, 72          ; 'H'
    STORE R0, 0
    MOV R0, 69          ; 'E'
    STORE R0, 1
    MOV R0, 76          ; 'L'
    STORE R0, 2
    STORE R0, 3         ; 'L' again
    MOV R0, 79          ; 'O'
    STORE R0, 4
    END
