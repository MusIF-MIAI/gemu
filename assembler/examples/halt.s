; halt.s — the canonical idle tail: spin on self, then halt.
;
; This mirrors the idle-halt tail observed when DUMP1/funktionalcpu runs to
; completion (see docs/ISA.md §3.3). With the default ORG 0x0000 it is directly
; loadable via ge_load_program() (which copies to mem[0] and starts at PO=0).

        ORG     0x0000

start:  HLT                 ; 0A 00  — stop the CPU
        JU      start       ; 47 F0 00 00 — if restarted, loop back to HLT
