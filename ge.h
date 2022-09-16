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
 *
 * @rPO: the program address regitster, i.e. the register used
 * to scan the positions of the memory in which the program instructions are
 * recorded. (p.118).
 * @rV1: the addresser for the first operand.
 * @rV2: the addresser for the second operand.
 * @rV3: the addresser for external instructions using channel 3
 * @rV4: the addresser for external instructions using channel 2
 * @rRI: 8-bit register used to store the photodisc codes.
 * @rL1: 16-bit used to store the length of the operands or for
 * information in transit
 * @rL2: 8-bit auxiliary register
 * @rL3: 16-bit register containing the length of operands involving
 * channel 3
 * @rNO: Knot driven by P0, V1, V2, V4, L1, R1, V3 and L3.
 *       In addition, the NO knot contains:
 *          - the forcings from program
 *          - the signals of forcing from console (AM switches)
 *
 * @rRO: multi-function 8+1 bit register that store the read signal from memory
 *      (e.g. the result of transfer command MEM).
 * @rV0: 16-bit register which is loaded in TO20 from NO, used to address memory
 *       for read and write operations.
 * @rBO: 16-bit register used to drive the UA (aka ALU) and used to visualize the
 *       content of other registers on the operating panel of the console
 * @rNI: Knot driven by counting network, or by the UA to store the result of the
 *       operation. UA may store MSB or LSB depending on the operation.
 * @rFO: 8-bit register storing the function code of the instruction being
 *       executed.
 *
 * @rSO: the main sequencer of the processor. It is used to establish the sequence
 *       for all the phases of program loading, fetching (phase alpha), executing
 *       (phase beta)
 *
 * @rSI: 4-bit sequencer used for data xechange with peripheral units through
 *       channel 2
 *
 * @rNA: Knot driven by SO or SI. Content is stored to SA.
 *
 * @rSA: Register that drives the MLS and the logic to generate future status
 *       configuration
 * @ffFI: 7 Flip-Flops containing special conditions which occur during the
 *      performance of an instruction. Unloaded in @FA in T010
 * @ffFA: 7 Flip-Flops containing special conditions which occur during the
 *      performance of an instruction. Loaded from @FI in T010
 * @AINI: FF It as the meaning of "Program Loading". It is set pressing "LOAD"
 * and it is reset pressing "CLEAR" or with command CI39 (E0 status alpha)
 * @counting_network: represent the GE counting network
 * @console: represent the GE console
 */
struct ge {
    /* Main clock */
    enum clock current_clock;
    uint64_t ticks;
    uint8_t halted;

    /* Lists of events and operations for all
     * pulses
     */
    struct pulse_event *on_pulse[MAX_CLOCK];

    /* Registers */

    /* Program addresser */
    uint16_t rPO;
    /* Argument addresser */
    uint16_t rV1;
    uint16_t rV2;
    uint16_t rV3;
    uint16_t rV4;

    /* Photoprint register */
    uint8_t rRI;

    uint16_t rL1;
    uint8_t rL2;
    uint16_t rL3;

    /* Knot temporary values */
    uint16_t kNO;
    uint8_t kNA;
    uint16_t kNI;


    /* General-purpose */
    uint16_t rRO;        /* length: 9 bits, using uint16_t */

    /* Default memory addresser, automatically loaded from NO*/
    uint16_t rVO;

    /* Default operator, automatically loaded from NO */
    uint16_t rBO;

    /* Current function code (opcode) */
    uint8_t rFO;

    /* Main sequencer */
    uint8_t rSO;

    /* Sequencer for the peripheral unit */
    uint8_t rSI; /* Four bits, using uint8_t */

    /* MLS/Future state configuration register */
    uint8_t rSA;

    /* Registers for special conditions and exceptions (7bits)*/
    uint8_t ffFI;
    uint8_t ffFA;

    uint8_t AINI:1;
    /* Faults: TODO (pp. 139-141) */

    /* Memory */
    uint8_t mem[MEM_SIZE];
};

/// Initialize the emulator
int ge_init(struct ge *ge);

/// Run the emulator
int ge_run(struct ge *ge);

/// Run a sigle cycle (all GE "mastri" clock periods)
int ge_run_cycle(struct ge *ge);

/// Emulate the press of the "clear" button in the console
void ge_clear(struct ge * ge);

/// Emulate the press of the "load" button in the console
int ge_load(struct ge * ge, uint8_t *program, uint8_t size);

/// Emulate the press of the "start" button in the console
int ge_start(struct ge * ge);


typedef int (*on_pulse_cb)(struct ge *);

struct pulse_event {
    on_pulse_cb cb;
    struct pulse_event *next;
};

/* Defined in pulse.c: execute pulse events */
int pulse(struct ge *ge);

#endif /* GE_H */
