/*
 * radiotune.c — OPER. CALL lamp radio music: the frequency sweep is the
 * lead voice.
 *
 * The lamp drive is the transmitter (pitch = toggle rate, timbre =
 * duty), and every "note" is a full accelerating glissando -- the bench
 * favourite sleep(t) chirp, promoted and mirrored:
 *
 *   falling sweep: period 0 -> d, step growing 1,2,3...   (pitch dives)
 *   rising  sweep: period d -> 0, same acceleration       (pitch climbs)
 *   trill:         rapid alternation of two periods       (ornament)
 *
 * Sweep-rate LFOs (all add-only; gec's mul/div/shift helpers would
 * blunt the fast end of the glide) reshape every sweep:
 *   m: mode pattern, 5-cycle  (fall, rise, fall, trill, rise)
 *   d: depth  60..240 by 30, 7-cycle -- sweeps get deeper, then reset
 *   c: duty colour 0..48 by 6, 9-cycle -- off-time offset (timbre)
 * 5 x 7 x 9 = 315 sweeps (~10+ minutes) before the piece repeats.
 * Every 8th sweep: a max-rate stab and a 500 ms breath (phrasing).
 *
 * Build:  gec radiotune.c --bootge -o radiotune.cap
 * Feed:   arm radiotune.cap ; CLEAR -> LOAD1 -> LOAD -> START
 */

int main()
{
    int p;          /* current period (ms)                  */
    int s;          /* sweep step, accelerating             */
    int d;          /* LFO: sweep depth (deepest period)    */
    int c;          /* LFO: duty colour (extra OFF ms)      */
    int m;          /* LFO: sweep-mode pattern              */
    int ph;         /* phrase counter                       */
    int i;

    d = 60; c = 0; m = 0; ph = 0;

    while (1) {
        if (m == 3) {
            /* ornament: trill between a high and a low period */
            i = 0;
            while (i < 20) {
                lon(); sleep(8);  loff(); sleep(8 + c);
                lon(); sleep(24); loff(); sleep(24 + c);
                i = i + 1;
            }
        } else if (m == 1 || m == 4) {
            /* rising sweep: period d -> 0, accelerating climb */
            p = d; s = 1;
            while (p > 0) {
                lon(); sleep(p); loff(); sleep(p + c);
                p = p - s;
                s = s + 1;
            }
        } else {
            /* falling sweep: period 0 -> d (the classic dive) */
            p = 0; s = 1;
            while (p < d) {
                lon(); sleep(p); loff(); sleep(p + c);
                p = p + s;
                s = s + 1;
            }
        }

        /* sweep done: the LFOs reshape the next one */
        m = m + 1;
        if (m >= 5) {
            m = 0;
        }
        d = d + 30;
        if (d > 240) {
            d = 60;
        }
        c = c + 6;
        if (c > 48) {
            c = 0;
        }
        ph = ph + 1;
        if (ph >= 8) {
            ph = 0;
            i = 0;
            while (i < 60) {   /* accent: max-rate stab */
                lon();
                loff();
                i = i + 1;
            }
            loff();
            sleep(500);        /* breath */
        }
    }
    return 0;
}
