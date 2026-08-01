; orchestra.s — the whole machine as the orchestra.
;
; Different operations energize different circuits, and the AM radio
; hears each as its own timbre. Four sections rotate, each "burning"
; its notes with a different instruction class:
;
;   sec 0  STRINGS  MVC 64-byte block copies   — sustained core R/W streams
;   sec 1  BRASS    AP packed-decimal adds     — the decimal ALU boards,
;                                                carry ripple + correction
;   sec 2  ORGAN    TL table translate         — scattered table-indexed
;                                                core reads (address noise)
;   sec 3  BELLS    XC 64-byte block XOR       — logic gating; core write
;                                                current depends on the bit
;                                                flips: data-DEPENDENT tone
;
; The conductor is the bytebeat engine from bytebeat.s:
;   v = ((t & 0x3FF) << 5) ^ (t << 2)     (overflow-folding resonance)
; whose high byte sets how many instrument strokes each note burns, so
; every timbre plays the same folding-sawtooth score. Sections rotate
; every 256 notes with a max-rate lamp stab; the lamp otherwise beats
; on bit 8 of t (tempo percussion).
;
; And it blossoms: ORGAN and BELLS mutate the 64-byte voice buffer
; (closed over values 0..63, so TL stays in range forever), so the
; data-dependent timbres evolve across the whole 65536-note cycle --
; the piece never plays the same section twice with the same data.
;
; Build:  gasm --bootge -o orchestra.cap orchestra.s
; Feed:   arm orchestra.cap ; CLEAR -> LOAD1 -> LOAD -> START

        ORG  0x1100

start:  MVI  0, t
        MVI  0, t+1
        MVI  0, sec

note:   AB   2,2, t+1, one+1     ; t += 1 (wraps: the grand cycle)

        MVC  2, v, t             ; v = ((t & 0x3FF) << 5) ^ (t << 2)
        NC   2, v, m3ff
        AB   2,2, v+1, v+1
        AB   2,2, v+1, v+1
        AB   2,2, v+1, v+1
        AB   2,2, v+1, v+1
        AB   2,2, v+1, v+1
        MVC  2, w, t
        AB   2,2, w+1, w+1
        AB   2,2, w+1, w+1
        XC   2, v, w
        MVC  1, cnt, v           ; strokes this note = v >> 8

        TM   0x01, t             ; lamp percussion on bit 8 of t
        JC   0x20, lampoff
        LON
        JU   play
lampoff:
        LOFF

play:   CMI  0, sec              ; dispatch the section's instrument
        JC   0x20, strings
        CMI  1, sec
        JC   0x20, brass
        CMI  2, sec
        JC   0x20, organ

bells:  CMC  1, cnt, zero        ; sec 3: XC block XOR (data-dependent)
        JC   0x20, adv
        XC   64, bufa, table
        SB   1,1, cnt, one+1
        JU   bells

strings: CMC 1, cnt, zero        ; sec 0: MVC block copy
        JC   0x20, adv
        MVC  64, bufb, bufa
        SB   1,1, cnt, one+1
        JU   strings

brass:  CMC  1, cnt, zero        ; sec 1: packed-decimal AP
        JC   0x20, adv
        AP   8,8, pk1+7, pk2+7
        SB   1,1, cnt, one+1
        JU   brass

organ:  CMC  1, cnt, zero        ; sec 2: TL translate (rotates bufa)
        JC   0x20, adv
        TR   64, bufa, table
        SB   1,1, cnt, one+1
        JU   organ

adv:    TM   0xFF, t+1           ; every 256 notes: change section
        JC   0x20, change
        JU   note
change: AB   1,1, sec, one+1
        CMI  4, sec
        JC   0x50, stab
        MVI  0, sec
stab:   MVI  20, i               ; section mark: max-rate lamp stab
stabl:  LON
        LOFF
        SB   1,1, i, one+1
        CMC  1, i, zero
        JC   0x50, stabl
        JU   note

; ---- engine state ----------------------------------------------------
t:      DW   0
v:      DW   0
w:      DW   0
cnt:    DB   0
sec:    DB   0
i:      DB   0
one:    DW   1
zero:   DB   0
m3ff:   DB   0x03, 0xFF

; ---- voices ----------------------------------------------------------
; 64-byte voice buffer, values 0..63 (closed under TL and XC below)
bufa:   DB   0,1,2,3,4,5,6,7
        DB   8,9,10,11,12,13,14,15
        DB   16,17,18,19,20,21,22,23
        DB   24,25,26,27,28,29,30,31
        DB   32,33,34,35,36,37,38,39
        DB   40,41,42,43,44,45,46,47
        DB   48,49,50,51,52,53,54,55
        DB   56,57,58,59,60,61,62,63
bufb:   DS   64                  ; STRINGS target (content irrelevant)
; rotate-by-one translate table over 0..63
table:  DB   1,2,3,4,5,6,7,8
        DB   9,10,11,12,13,14,15,16
        DB   17,18,19,20,21,22,23,24
        DB   25,26,27,28,29,30,31,32
        DB   33,34,35,36,37,38,39,40
        DB   41,42,43,44,45,46,47,48
        DB   49,50,51,52,53,54,55,56
        DB   57,58,59,60,61,62,63,0
; packed-decimal brass reeds
pk1:    DB   0x12,0x34,0x56,0x78,0x90,0x12,0x34,0x5C
pk2:    DB   0x00,0x00,0x00,0x00,0x00,0x00,0x07,0x7C
