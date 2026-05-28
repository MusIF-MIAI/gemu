; mvc.s — move a small character field, set an immediate, then halt.
;
; Demonstrates the SS single-length form (MVC), the PM immediate form (MVI),
; data definition (DB), labels used as addresses, and the halt tail.

        ORG     0x0000

start:  MVI     'A', dst        ; mem[dst] <- 0x41
        MVC     5, dst, src     ; copy 5 chars  src -> dst   (LL = 4)
        HLT
        JU      start

        ORG     0x0040
src:    DB      "HELLO"         ; 5 source bytes
dst:    DS      5               ; 5 reserved destination bytes
