; GE-120/130 self-loading scatter loader card  (CPU[1] §3.8 folio 19,
; "Loader cards program listing — Coding of loader cards for LS or CR10").
; Transcribed verbatim from the rendered page image (4T4719200WA3, 130 CPU
; FUNCTIONAL TEST). The IPL (CLEAR+LOAD+START) reads this card nibble-packed to
; 0x0000 and runs it; it then PER-reads each following binary program card into
; 0x0036, relocates the payload to the card's embedded load address (8-col
; header: col8=LL, cols9-10=II, cols11+=payload), and loops.
        ORG 0x0000
        PER 0x80, 0x0022        ; Set by-Pass (switch reader to binary 1col/byte)
        PER 0x80, 0x0020        ; Read program card -> 0x0036
        PER 0x80, 0x0026        ; Error exam
        JC  0x10, 0x0015        ; JG -> error HLT
        MVC 3, 0x0017, 0x003E   ; copy LL+II from card header into the MVC below
        MVC 1, 0x0000, 0x0041   ; relocate payload -> II  (LL/dst patched above)
        JU  0x0000              ; read the following card
        ORG 0x0020
        DB 0x00, 0x40, 0x80, 0x40, 0x00, 0x36   ; read order  (cmd 0x40, buf 0x0036)
        DB 0x00, 0x03                            ; exam order
