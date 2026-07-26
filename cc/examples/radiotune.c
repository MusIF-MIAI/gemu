/*
 * radiotune.c — OPER. CALL lamp radio music, evolved from the bench
 * favourite (the accelerating sleep(t) chirp).
 *
 * The lamp drive is the transmitter: every LON/LOFF edge is what the
 * nearby AM radio picks up, so pitch = toggle rate and timbre = duty.
 *
 * Two detuned chirp oscillators share the lamp:
 *   voice A sweeps the ON  time  (the melody chirp),
 *   voice B sweeps the OFF time  (duty colour + sub-octave beating —
 *   as A and B drift against each other the duty cycle sweeps through
 *   every ratio, and their different ceilings make wide harmonics).
 *
 * Add-only LFOs on top (no mul/div in the hot path — gec compiles
 * those to slow helper loops that would blunt the fast chirp start):
 *   d1: register LFO, re-tunes voice A's ceiling every beat (5-cycle)
 *   d2: phrase LFO, every 8th beat fires an accent — a burst of
 *       maximum-rate toggles (high stab) — then a 400 ms breath.
 *   voice B's ceiling drifts 55..120 on its own wrap: slow beating.
 *
 * Build:  gec radiotune.c --bootge -o radiotune.cap
 * Feed:   arm radiotune.cap ; CLEAR -> LOAD1 -> LOAD -> START
 */

int main()
{
    int a;          /* voice A phase: lamp ON  ms */
    int astep;
    int areg;       /* A ceiling = melodic register */
    int b;          /* voice B phase: lamp OFF ms */
    int bstep;
    int breg;
    int d1;         /* LFO 1: register selector, wraps at 5  */
    int d2;         /* LFO 2: phrase counter,   wraps at 8  */
    int k;
    int i;

    a = 0; astep = 1; areg = 100;
    b = 0; bstep = 1; breg = 73;
    d1 = 0; d2 = 0;

    while (1) {
        lon();
        sleep(a);
        loff();
        sleep(b);

        /* voice A: the accelerating chirp (pitch) */
        a = a + astep;
        if (a >= areg) {
            a = 0;
            astep = 0;

            /* end of beat: run the LFOs (adds only) */
            d1 = d1 + 1;
            if (d1 >= 5) {
                d1 = 0;
            }
            /* register LFO: areg = 40 + 30*d1, by repeated add */
            areg = 40;
            k = 0;
            while (k < d1) {
                areg = areg + 30;
                k = k + 1;
            }

            d2 = d2 + 1;
            if (d2 >= 8) {
                d2 = 0;
                /* phrase accent: max-rate stab, then a breath */
                i = 0;
                while (i < 60) {
                    lon();
                    loff();
                    i = i + 1;
                }
                loff();
                sleep(400);
            }
        } else {
            astep = astep + 1;
        }

        /* voice B: detuned chirp on the OFF time (duty + beating) */
        b = b + bstep;
        if (b >= breg) {
            b = 0;
            bstep = 0;
            breg = breg + 18;      /* slow drift against voice A */
            if (breg >= 120) {
                breg = 55;
            }
        } else {
            bstep = bstep + 1;
        }
    }
    return 0;
}
