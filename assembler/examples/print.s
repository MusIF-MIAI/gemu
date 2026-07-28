; print.s — print "HELLO" on the integrated typewriter (channel 2).
;
; Issues a channel-2 output PER whose order block selects a put/print of 5
; characters from 0x0200, then waits for gemu's completion cell to flip. The
; printer detects the order, arms the transfer engine, and the rSI output
; microcode drains the buffer to the typewriter (GE 100-series graphic set) on
; stolen channel-2 cycles while the CPU polls.
;
;   gasm -o print.cap print.s
;
; then put the deck in the hopper and run CLEAR -> LOAD1 -> LOAD -> START.
; It lives at 0x0100 because the scatter loader and its card buffer occupy
; 0x0000-0x0085: on this machine only a 40-byte boot card can sit at 0.

        ORG     0x0100
        PER     0x80, order     ; connector-2 output PER, order block @ order
wait:   CMI     0x01, 0x0031    ; low byte of __io_status / printer done flag
        JNE     wait            ; wait until the channel-2 transfer ends
        HLT                     ; halt cleanly once HELLO has drained

        ORG     0x0110
order:  DB      0x81            ; Z   : bit 00 set = channel 2, the only
                                ;       channel the integrated printer can be
                                ;       reached on (CPU[4] 5.8.3.1, fo.73)
        DB      0x85            ; X   : put / print
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
