; boot.s — the IPL boot card TEMPLATE: pull the rest of the deck to
; DEST, one card per PER read, then jump there.
;
; Linked automatically by  gasm --boot -o prog.cap prog.s : gasm
; assembles the program first, then this file with DEST/DONE/NCARDS
; overridden to the program's real origin and size (the EQUs below are
; standalone defaults; --boot's values win), and emits a ready .cap
; deck -- this card hex-encoded first, the program as raw 80-byte
; COLBIN body cards after it. Feed with  arm prog.cap .
;
; Standalone use is still possible:  gasm --card -o boot.bin boot.s
; (38/40 bytes; edit the EQUs by hand -- gasm expressions have no '*':
; DONE = DEST + NCARDS*80).
;
; Convention: the body cards carry NO headers -- each PER reads one card
; (80 bytes) STRAIGHT to its destination by patching the read order's V1
; in place; no buffer, no relocate MVC.
;
; Techniques, all decoded from the historical one-card loader
; (funktionalcpu deck / CPU[1] par. 3.8):
;   - PER order format [Z][RE][L1][V1]: Z=0 RE=0x40 = read, L1=0x80A0 =
;     160 nibble transfers (80 bytes) with the bit-15 flag, V1 = target.
;   - self-modification of the order field between reads.
;   - register-op memory operands address the RIGHTMOST byte of the
;     16-bit word, hence the '+1' on every constant (ISA.md; verified
;     against funktionalcpu step 0x37).
;
; Caveats: no error/exam handling (2 bytes spare would not fit it) and
; change register 0 (mem 0xF0-F1) is left holding DONE -- reload it (LA)
; if the loaded program uses indexed addressing on base 0.

NCARDS  EQU  2                 ; body cards to pull      (EDIT per deck)
DEST    EQU  0x1000            ; load base = entry point
DONE    EQU  0x10A0            ; = DEST + NCARDS*80      (EDIT per deck)

        ORG  0

start:  LA   0, DEST           ; reg0 = next card's destination
loop:   STR  0, rdaddr+1       ; patch the read order's V1
        PER  0x80, rdord       ; read one card -> [reg0 .. reg0+79]
        AMR  0, k80+1          ; reg0 += 80
        CMR  0, done+1         ; all cards in?
        JNE  loop
        JU   DEST              ; run it

rdord:  DB   0x00, 0x40        ; Z=0, RE=0x40: read one card
        DW   0x80A0            ; L1: 160 nibbles = 80 bytes
rdaddr: DW   0                 ; V1: destination (patched each pass)
k80:    DW   80
done:   DW   DONE
