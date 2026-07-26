; bytebeat-sw.s — the radio bytebeat with SWITCH 1 / SWITCH 2 on the
; console selecting the equation, live, per note. The operator performs.
;
;   SW1 SW2   voice
;    0   0    v = ((t & 0x3FF) << 5) ^ (t << 2)     the classic
;    1   0    v = ((t & 0x1FF) << 6) ^ (t << 3)     bright: narrow saw,
;                                                   double fold rate
;    0   1    v = ((t & 0x7FF) << 4) ^ (t << 6)     wide: deep slow saw
;                                                   under a fast shimmer
;    1   1    v = (v << 1) ^ (t << 2) ^ (t & 0x333) feedback: v is not
;                                                   reset from t, so the
;                                                   voice chews on its own
;                                                   history -- chaos pad
;
; Switches are sampled EVERY note (JS1/JS2), so flipping them morphs the
; piece without a glitch; the tone loop and the lamp (bit 8 of t, in
; tempo) are common to all voices. Same instrument as bytebeat.s: the
; tone loop burns v>>8 iterations per sample and the AM radio hears the
; loop-rate envelope; << is AB self-addition (mod-2^16 folds = the
; overflow resonance), & is NC with a mask.
;
; In gemu:  ge bytebeat-sw.bin -i   then  kill -USR1/-USR2 <pid>
; On iron:  gasm --bootge -o bytebeat-sw.cap bytebeat-sw.s
;           arm bytebeat-sw.cap ; CLEAR -> LOAD1 -> LOAD -> START
;           and play the SWITCH 1 / SWITCH 2 keys.

        ORG  0x1100

start:  MVI  0, t
        MVI  0, t+1
        MVI  0, v
        MVI  0, v+1

loop:   AB   2,2, t+1, one+1     ; t += 1

        JS1  sw1on               ; sample the console switches
        JS2  eq01
        JU   eq00
sw1on:  JS2  eq11
        JU   eq10

eq00:   MVC  2, v, t             ; ((t & 0x3FF) << 5) ^ (t << 2)
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
        JU   lamp

eq10:   MVC  2, v, t             ; ((t & 0x1FF) << 6) ^ (t << 3)
        NC   2, v, m1ff
        AB   2,2, v+1, v+1
        AB   2,2, v+1, v+1
        AB   2,2, v+1, v+1
        AB   2,2, v+1, v+1
        AB   2,2, v+1, v+1
        AB   2,2, v+1, v+1
        MVC  2, w, t
        AB   2,2, w+1, w+1
        AB   2,2, w+1, w+1
        AB   2,2, w+1, w+1
        XC   2, v, w
        JU   lamp

eq01:   MVC  2, v, t             ; ((t & 0x7FF) << 4) ^ (t << 6)
        NC   2, v, m7ff
        AB   2,2, v+1, v+1
        AB   2,2, v+1, v+1
        AB   2,2, v+1, v+1
        AB   2,2, v+1, v+1
        MVC  2, w, t
        AB   2,2, w+1, w+1
        AB   2,2, w+1, w+1
        AB   2,2, w+1, w+1
        AB   2,2, w+1, w+1
        AB   2,2, w+1, w+1
        AB   2,2, w+1, w+1
        XC   2, v, w
        JU   lamp

eq11:   AB   2,2, v+1, v+1       ; (v << 1) ^ (t << 2) ^ (t & 0x333)
        MVC  2, w, t             ;   -- v carries over: feedback chaos
        AB   2,2, w+1, w+1
        AB   2,2, w+1, w+1
        XC   2, v, w
        MVC  2, w, t
        NC   2, w, m333
        XC   2, v, w

lamp:   TM   0x01, t             ; lamp in tempo (bit 8 of t)
        JC   0x20, lampoff
        LON
        JU   mix
lampoff:
        LOFF

mix:    MVC  1, cnt, v           ; sample = v >> 8
tone:   CMC  1, cnt, zero
        JC   0x20, loop
        SB   1,1, cnt, one+1
        JU   tone

t:      DW   0
v:      DW   0
w:      DW   0
cnt:    DB   0
one:    DW   1
zero:   DB   0
m3ff:   DB   0x03, 0xFF
m1ff:   DB   0x01, 0xFF
m7ff:   DB   0x07, 0xFF
m333:   DB   0x03, 0x33
