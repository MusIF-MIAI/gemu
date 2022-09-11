#ifndef GE_H
#define GE_H

#include <stdint.h>

#define MEM_SIZE 65536

enum clock {
    TO00 = 0,
    TO10,
    TO11,
    TO15,
    TO19,
    TO20,
    TO25,
    TO30,
    TO40,
    TO50,
    TO64,
    TO70,
    TO80,
    TO89,
    TO90,
    TI05,
    TI06,
    TI10,
    MAX_CLOCK
};

/**
 * struct ge
 * @rSO: the SO register is the main sequencer of the processor. It is loaded by
 * th eoutputs of the logic network of the future status (SU00 - SU07).It drives
 * the NA knot when the cycle has been attributed to CPU or channel 1 (p. 126).
 */
struct ge {
    enum clock current_clock;
    uint8_t mem[MEM_SIZE];
    uint64_t ticks;
    uint8_t rSO;
    /* ge state here */
};

int ge_init(struct ge *ge);
int ge_run(struct ge *ge);
int ge_run_cycle(struct ge *ge);

#endif /* GE_H */
