; print.s — print "HELLO" on the integrated typewriter (channel 2).
;
; Issues a channel-2 output PER whose order block selects a put/print of 5
; characters from 0x0200, then waits for gemu's completion cell to flip. The
; printer detects the order, arms the transfer engine, and the rSI output
; microcode drains the buffer to the typewriter (GE 100-series graphic set) on
; stolen channel-2 cycles while the CPU polls. Load it in the wasm console
; (Stage -> CLEAR -> LOAD -> START) and watch the Printer panel print HELLO.

        ORG     0x0000
        PER     0x80, order     ; connector-2 output PER, order block @ order
wait:   CMI     0x01, 0x0031    ; low byte of __io_status / printer done flag
        JNE     wait            ; wait until the channel-2 transfer ends
        HLT                     ; halt cleanly once HELLO has drained

        ORG     0x0010
order:  DB      0x80            ; z   : L207 (output)
        DB      0x85            ; cmd : put / print
        DB      0x00            ; length high
        DB      0x05            ; length low  (5 characters)
        DB      0x02            ; buffer high
        DB      0x00            ; buffer low  (0x0200)

        ORG     0x0200
text:   DB      0x58            ; 'H'  (GE graphic code)
        DB      0x55            ; 'E'
        DB      0xA3            ; 'L'
        DB      0xA3            ; 'L'
        DB      0xA6            ; 'O'
