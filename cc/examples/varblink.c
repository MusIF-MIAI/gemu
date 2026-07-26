/*
 * varblink.c — sweep the OPER. CALL blink period back and forth between
 * "no sleep at all" (loop-speed flicker, the lamp looks half-lit) and a
 * stately 2000 ms, adjusting by STEP after every on/off pair.
 *
 * Build a deck:  gec varblink.c --bootge -o varblink.cap
 * Feed:          arm varblink.cap ; CLEAR -> LOAD1 -> LOAD -> START
 */

int main()
{
    int t;
    int step;

    t = 0;
    step = 100;
    while (1) {
        lon();
        sleep(t);
        loff();
        sleep(t);

        t = t + step;
        if (t >= 2000) {
            t = 2000;
            step = 0 - step;
        }
        if (t <= 0) {
            t = 0;
            step = 0 - step;
        }
    }
    return 0;
}
