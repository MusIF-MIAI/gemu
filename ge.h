#ifndef GE_H
#define GE_H

#include <stdint.h>
#include "opcodes.h"
#include "console.h"

#define CLOCK_PERIOD 14000 /* in usec, interval between pulse lines */
#define MEM_SIZE 65536

#define ENUMERATE_CLOCKS \
    X(TO00) \
    X(TO10) \
    X(TO11) \
    X(TO15) \
    X(TO19) \
    X(TO20) \
    X(TO25) \
    X(TO30) \
    X(TO40) \
    X(TO50) \
    X(TO60) \
    X(TO64) \
    X(TO65) \
    X(TO70) \
    X(TO80) \
    X(TO89) \
    X(TO90) \
    X(TI05) \
    X(TI06) \
    X(TI10) \
    X(END_OF_STATUS)

enum clock {
    #define X(name) name ,
    ENUMERATE_CLOCKS
    #undef X
};

struct ge_counting_network {
    uint16_t output;
    struct cmds {
        uint8_t from_zero:1;
    } cmds;
};

/**
 * The entire state of the emulated system, including registers, memory,
 * peripherals and timings.
 */
struct ge {
    /* Main clock */
    enum clock current_clock;
    uint8_t halted;
    uint8_t powered;

    /**
     * SO at the start of the pulse
     *
     * Used to stop the machine when we do not change state after a cycle,
     * for debuggiong reasons.
     */
    int old_SO;

    /* Lists of events and operations for all
     * pulses
     */
    struct pulse_event *on_pulse[END_OF_STATUS];

    /**
     * Program addresser.
     *
     * The register used to scan the positions of the memory in which
     * the program instructions are recorded. (p.118).
     */
    uint16_t rPO;

    uint16_t rV1; ///< Addresser for the first operand
    uint16_t rV2; ///< Addresser for the second operand
    uint16_t rV3; ///< Addresser for external instructions using channel 3
    uint16_t rV4; ///< Addresser for external instructions using channel 2

    /**
     * Photoprint register
     * 8-bit register used to store the photodisc codes.
     */
    uint8_t rRI;

    /**
     * Length of the operand
     *
     * 16-bit used to store the length of the operands or for information in
     * transit.
     */
    uint16_t rL1;
    uint8_t  rL2; ///< Auxiliary register
    uint16_t rL3; ///< Length of operands involving channel 3

    /**
     * Knot driven by P0, V1, V2, V4, L1, R1, V3 and L3.
     *
     * In addition, the NO knot contains:
     *   - the forcings from program
     *   - the signals of forcing from console (AM switches)
     */
    uint16_t kNO;

    /**
     * Knot driven by SO or SI. Content is stored to SA.
     *
     * Driven
     *  - by the SO register when the work cycle has been attributed to the
     *    CPU or to channel 1, if the rotary switch is the "central" position
     *    (AF326=1)
     *  - by the SI register (less four significant bits) when the cycle has
     *    been attributed to channel 2
     *
     * Additionally, individual bits might be set
     *  - NA00: forced to 1 when the work cycle is attributed to channel 1 or
     *    channel 3
     *  - NA03: forced to 1 when the work cycle has been attributed to the CPU
     *    and the rotary switch is not in the "central" position (AF32C = 1))
     *
     * Stored in SA register during T010. (cpu fo. 128)
     */
    uint8_t kNA;

    /**
     * Knot driven by counting network, or by the UA to store the result of the
     * operation. UA may store MSB or LSB depending on the operation.
     */
    uint16_t kNI;

    /**
     * Multipurpose 8+1 bit register
     *
     * Stores the read signal from memory (e.g. the result of transfer command MEM).
     */
    uint16_t rRO;

    /**
     * Default memory addresser
     *
     * 16-bit register which is loaded in TO20 from NO, used to address memory for
     * read and write operations.
     */
    uint16_t rVO;

    /**
     * Default operator
     *
     * 16-bit register automatically loaded from NO, used to drive the UA (aka ALU)
     * and used to visualize the content of other registers on the operating panel of
     * the console
     */
    uint16_t rBO;

    /**
     * Current function code
     *
     * 8-bit register storing the function code of the instruction being executed.
     */
    uint8_t rFO;

    /**
     * Main sequencer
     *
     * Drives the kNA knot when the cycle has been attributed to the CPU or
     * channel 1.
     *
     * It is used to establish the sequence for:
     *  - alpha phase for all internal and external instructions
     *  - beta phase of internal instructions
     *  - organisation phase (general beta) of external instructions
     *  - program loading
     *
     * Loaded from the future status network when signal SOC01 is activated,
     * provided the RICI key is not active, in the following cases:
     *  - the FF ARES has been set thru "CLEAR". This causes the machine to
     *    execute the status 00, and setting of SO07 using CU07, this will
     *    set the configuration of SO to 80.
     *  - the rotary switch is in forcing of SO. When a cycle is attributed
     *    to the CPU pressing "START", the AM00-07 keys are forced in SO.
     *  - at the end of a cycle attributed to the CPU, when the rotary switch
     *    is in normal position, the future status network is stored in SO.
     *  (cpu fo. 127)
     */
    uint8_t rSO;

    /**
     * Peripheral unit sequencer
     *
     * 4-bit sequencer used for data xechange with peripheral units through
     * channel 2.
     *
     * Drives the kNA knot when the cycle has been attributed to channel 2.
     *
     * Loaded with the first 4 bits of the future status network
     *  - after the execution of a channel 2 cycle
     *  - when forcing a status in SI using CU20 (DC status of general beta phase)
     */
    uint8_t rSI;

    /**
     * Future state configuration
     *
     * Register that drives the MLS and the logic to generate future status
     * configuration
     */
    uint8_t rSA;

    /**
     * Special conditions register 1
     *
     * 7 Flip-Flops containing special conditions which occur during the performance
     * of an instruction. Unloaded in #ffFA in T010
     */
    uint8_t ffFI;

    /**
     * Special conditions register 2
     *
     * 7 Flip-Flops containing special conditions which occur during the performance
     * of an instruction. Loaded from #ffFI in T010.
     *
     * Faults (from pp. 139-141)
     */
    uint8_t ffFA;

    /* Those two store which work channel the present cycle as been attributed to
     * (cpu fo. 130).
     * When setting RETO, if PAPA switch is set, rotary is neither in normal, nor
     * in position 8 to store in memory, should set ALTO */
    uint8_t ffRETO:1;
    uint8_t ffRET2:1;

    /**
     * Program Loading
     *
     * Set by pressing the "LOAD" button of the console, and it is reset by pressing
     * "CLEAR", or with the command CI39 (in the alpha phase of the E0 state).
     */
    uint8_t AINI:1;

    /**
     * Stops internal cycles
     *
     * If set, stops the performance of the internal processing cycles, without
     * stopping the timing generation (cpu fo. 98).
     */
    uint8_t ALTO:1;

    /**
     * Slow delay line
     *
     * Increases the delay line cycle by about 130ns. It is set together with
     * ALAM by the LOLL diagnostic instruction (cpu fo. 96).
     */
    uint8_t PODI:1;

    /**
     * Recycle delay line
     *
     * Initially is set by "CLEAR", after that it is reset cyclicly. The reset
     * pulse is  TO10, the normal setting pulse is TO90 if a LOLL instruction
     * has not been performed, in this case it is set by TI05, with a delay
     * of about 130ns. (cpu fo. 96).
     *
     * NOTE: documentation differs at cpu fo. 99 that states:
     *
     * The ACIC1 FF is reset by the TO10 pulse and it is set by the TO901 with
     * the condition PODIB == 1.
     *
     * PODI is the FF which stores the LOLL diagnostic instruction performance
     * causing an increase of the cycle of about 130 ns.
     *
     * In fact, if PODIB == 0 the recycling occurs with the pulse TI05 instead
     * of TO90.
     */
    uint8_t ACIC:1;

    /**
     * Operator Call
     *
     * It commands the switching on of the "Operator call" lamp. It is set with
     * CI87 issued by the LON and LOLL instructions.
     * It is reset with CI88 issued by the LOFF instruction, or by pressing the
     * "CLEAR" button (cpu fo. 96).
     */
    uint8_t ALAM:1;

    /**
     * Jump Condition Verified
     *
     * Reset in the E0 status of the alpha phase, together with AINI, with the
     * CI39 command (cpu fo. 96).
     *
     * Set in the E6 status of the alpha phase of the jump instructions (CI38)
     * if signal DC16 (verified condition) is present (cpu fo. 96).
     */
    uint8_t AVER:1;

    /**
     * Disable Step By Step
     *
     * Set with CI77 by the INS instruction, reset with CI78 issued by ENS, or
     * with "CLEAR" (cpu fo. 97).
     */
    uint8_t ADIR:1;

    uint8_t RINT:1;

    uint8_t JS1:1;  ///< Console jump condition 1
    uint8_t JS2:1;  ///< Console jump condition 2
    uint8_t JE:1;   ///< JE/AVER jump instruction exectuted
    uint8_t INTE:1; ///< Interruption present

    /**
     * Selection Channel 1
     *
     * Used during the general B phasef o rcommand forwarding or condition
     * examination.
     * Unconditionally set by command CE02 which enables the channel selection
     * even if the interested  channels are 2 or 3.
     * When a character transfer in output has been initiated with channel 1,
     * signal PAP4A resets PUC1 at the start of the transper phase, when the
     * first transfer is done from RO in to RA (CE00) unless signal PAR21 had
     * already absolved this function. (cpu fo. 235)
     */
    uint8_t PUC1:1;

    /**
     * Channel 1 in transfer
     *
     * (cpu fo. 236)
     */
    uint8_t RASI:1;

    /**
     * Channel 2 in transfer
     *
     * (cpu fo. 236)
     */
    uint8_t PUC2:1;

    /**
     * Channel 3 in transfer
     *
     * (cpu fo. 236)
     */
    uint8_t PUC3:1;

    uint8_t URPE:1;
    uint8_t URPU:1;

    /**
     * The current state of the console register rotary switch
     */
    enum ge_console_rotary register_selector;

    /**
     * The current state of the console switches
     */
    struct ge_console_switches console_switches;

    uint8_t step_by_step:1;  ///< Step by step execution @todo replace with signal name
    uint8_t mem[MEM_SIZE]; ///< The memory of the emulated system

    struct ge_counting_network counting_network;
    struct ge_peri *peri;
};

/// Initialize the emulator
void ge_init(struct ge *ge);

/// Deinitialize the emulator
int ge_deinit(struct ge *ge);

/// Copy a program at the start of memory
int ge_load_program(struct ge *ge, uint8_t *program, uint8_t size);

/// Run the emulator
int ge_run(struct ge *ge);

/// Run a single pulse (i.e. a single GE "mastri" clock periods)
int ge_run_pulse(struct ge *ge);

/// Run all GE "mastri" clock periods until next clock cycle
int ge_run_cycle(struct ge *ge);

/// Emulate the press of the "clear" button in the console
void ge_clear(struct ge * ge);

/// Emulate the press of the "load" button in the console
void ge_load(struct ge * ge);

/// Emulate the press of the "start" button in the console
void ge_start(struct ge * ge);

typedef void (*on_pulse_cb)(struct ge *);

struct pulse_event {
    on_pulse_cb cb;
    struct pulse_event *next;
};

/* Defined in pulse.c: execute pulse events */
void pulse(struct ge *ge);

int ge_struct_sizeof(void);

struct ge_peri {
    struct ge_peri *next;
    int (*init)(struct ge*, void*);
    int (*on_pulse)(struct ge*, void*);
    int (*on_clock)(struct ge*, void*);
    int (*deinit)(struct ge*, void*);
    void *ctx;
};

int ge_register_peri(struct ge *ge, struct ge_peri *p);

#endif /* GE_H */
