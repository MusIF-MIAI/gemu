; halt.s — the canonical idle tail: spin on self, then halt.
;
; This mirrors the idle-halt tail observed when DUMP1/funktionalcpu runs to
; completion (see docs/ISA.md §3.3). Six bytes at ORG 0x0000, so it is a boot
; card in its own right:  gasm --card -o halt.cap halt.s

        ORG     0x0000

start:  HLT                 ; 0A 00  — stop the CPU
        JU      start       ; 47 F0 00 00 — if restarted, loop back to HLT
