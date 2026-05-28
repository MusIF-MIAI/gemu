; regs.s — change-register (segment-base) manipulation.
;
; The eight change registers are memory-mapped 16-bit big-endian words at
; 0x00F0 + 2N (N = 0..7) and default to identity bases N<<12.  LA loads a
; register with an effective address; LR loads it from memory; STR stores it.
; Here we point base register 2 at a 4 KiB window and compare it.

        ORG     0x0000

start:  LA      2, buf          ; reg[2] <- address of buf
        STR     2, save         ; mem16[save] <- reg[2]
        LR      3, save         ; reg[3] <- mem16[save]
        CMR     3, save         ; compare reg[3] with mem16[save]; set CC
        HLT
        JU      start

        ORG     0x0040
save:   DW      0x0000          ; 2-byte scratch word
buf:    DS      16              ; a little buffer
