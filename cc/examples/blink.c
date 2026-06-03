/*
 * blink.c — blink the operator-call (OPER. CALL) lamp at ~1 Hz.
 *
 * gec built-ins used (no header needed — they ride on real CPU instructions):
 *   lon()       light the operator-call lamp    (LON : CI87 sets ALAM)
 *   loff()      extinguish it                    (LOFF: CI88 resets ALAM)
 *   sleep(ms)   busy-wait the given milliseconds (250 emulator cycles = 1 ms,
 *               i.e. ~500 ms == 125000 instruction clocks at 4 us/cycle)
 *
 * The lamp flip-flop (ALAM) is write-only to the program, so the on/off state
 * is kept in a variable and toggled each pass. Runs forever — stop from the
 * console.
 */
int main()
{
    int on;

    on = 0;
    while (1) {
        sleep(500);          /* ~500 ms == 125000 instruction clocks */
        if (on) {
            loff();
            on = 0;
        } else {
            lon();
            on = 1;
        }
    }
    return 0;
}
