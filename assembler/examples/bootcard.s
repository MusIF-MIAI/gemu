; bootcard.s — the minimal IPL boot card: a self-jump the address lamps show.
;
; Assemble with  gasm --card -o smoke.bin bootcard.s  and feed it on the
; card-reader emulator with  arm smoke.bin@0 : the machine's IPL
; (CLEAR + LOAD + START) nibble-packs this single 80-column hex card to
; 0x0000 and executes it there. A boot card is at most 40 bytes; anything
; bigger goes through the scatter loader instead (arm <file>@0x100).
;
; Bench-verified on the GE-120: the PC spinning at 0x0000-0x0003 is the
; shortest possible proof that a fed card runs.

        ORG     0x0000

loop:   JU      loop        ; 47 F0 00 00 — jump to self, forever
