; bytebeat.s — a radio-audible bytebeat for the GE-120.
;
; Put an AM radio (any quiet spot on the dial) near the backplane: the
; machine's bus/core switching is the carrier, and this loop modulates
; its duty at audio rate. The voice is
;
;       v = ((t & 0x3FF) << 5) ^ (t << 2)
;
; i.e. the classic  (t - ((t>>10)<<10)) << 5  sawtooth, XORed with a
; faster ramp. The ISA has no shifts: & is NC with a mask, << is
; repeated AB self-addition -- and AB wraps mod 2^16, so the doubled
; ramps fold over symmetrically (the "overflow resonance": each fold
; retunes the XOR interference pattern). The tone loop then burns
; v>>8 iterations per sample, so the outer-loop repetition rate -- the
; thing the radio hears -- sweeps 40 Hz-ish to a few kHz in XOR-ramp
; arpeggios. The whole piece loops every 65536 samples as t wraps.
;
; The OPER. CALL lamp follows bit 8 of t: it blinks in tempo with the
; music, fast in the high passages, slow in the low ones.
;
; Build a deck:  gasm --bootge -o bytebeat.cap bytebeat.s
; Feed:          arm bytebeat.cap ; CLEAR -> LOAD1 -> LOAD -> START
;
; Register-free: everything is memory-to-memory (SS ops), so no change
; register is touched and no reset-identity is assumed. Restarting via
; CLEAR + START replays from t = 0.

        ORG  0x1100

start:  MVI  0, t              ; deterministic restart: t = 0
        MVI  0, t+1

loop:   AB   2,2, t+1, one+1   ; t += 1  (wraps: the piece loops)

        MVC  2, v, t           ; v = t
        NC   2, v, m3ff        ; v &= 0x03FF        (t - (t>>10<<10))
        AB   2,2, v+1, v+1     ; v <<= 1
        AB   2,2, v+1, v+1
        AB   2,2, v+1, v+1
        AB   2,2, v+1, v+1
        AB   2,2, v+1, v+1     ; ... << 5 total, folding mod 2^16

        MVC  2, w, t           ; w = t << 2, the second ramp
        AB   2,2, w+1, w+1
        AB   2,2, w+1, w+1
        XC   2, v, w           ; v ^= w  (the XOR arpeggio)

        TM   0x01, t           ; bit 8 of t: lamp in tempo
        JC   0x20, lampoff
        LON
        JU   mix
lampoff:
        LOFF

mix:    MVC  1, cnt, v         ; sample = v >> 8 (high byte)
tone:   CMC  1, cnt, zero
        JC   0x20, loop        ; burned down: next sample
        SB   1,1, cnt, one+1   ; cnt -= 1
        JU   tone

t:      DW   0
v:      DW   0
w:      DW   0
cnt:    DB   0
one:    DW   1
zero:   DB   0
m3ff:   DB   0x03, 0xFF
