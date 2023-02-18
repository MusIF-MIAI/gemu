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
    X(TO50_1) \
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

struct ge_knot_no {
    uint8_t forcings;

    enum {
        KNOT_PO_IN_NO,
        KNOT_V1_IN_NO,
        KNOT_V2_IN_NO,
        KNOT_V3_IN_NO,
        KNOT_V4_IN_NO,
        KNOT_L1_IN_NO,
        KNOT_L2_IN_NO,
        KNOT_L3_IN_NO,
        KNOT_FORCE_IN_NO_21,
        KNOT_FORCE_IN_NO_43,
        KNOT_AM_IN_NO,
        KNOT_RI_IN_NO_43,
    } cmd;
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

    struct ge_knot_no kNO;

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
     * Drives the NA knot when the cycle has been attributed to the CPU or
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
     * Drives the NA knot when the cycle has been attributed to channel 2.
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

    /* Cycle Attribution Logic */
    /* ----------------------- */

    /* Asyncronous flip flops */

    /**
     * Asynchronous CPU Cycle Request
     *
     * It is reset with CE18 (enable RIAP) while a cycle is performed
     * for the CPU (RIUC=1). The CPU is thus waiting for the external
     * triggers of the command received.
     * It is set by the clear signal (CAGUF=0) with the signal of
     * command received by the peripheral unit (RBII1=1) with the
     * insertion of the SITE key which frees the waitings (RAITI=1)
     * and finally with the disselection of channel 1 (PU16 = 0)
     * (cpu fo. 114).
     */
    uint8_t RC00:1;

    /**
     * Asynchronous Channel 1 Cycle Request
     *
     * It is set with the OR of the channel 1 request triggers (RAI01)
     * if the executing instruction is not over (RIVEF=1).
     * Also, when the SITE key is inserted during a during a transfer of
     * channel 1 (RAISI2=1).
     * It is reset during a cycle of channel 1 with CE18 (enable RIAP),
     * or at the end of a transfer on channel 1
     * (cpu fo. 114)
     */
    uint8_t RC01:1;

    /**
     * Asynchronous Channel 2 Cycle Request
     *
     * It is set with the trigger LU08 from the integrated reader, or
     * when the SITE key is inserted (RAITI1=1) during the transfers
     * on channel 2.
     *
     * Request from printer do not act on RC02, but are derived from it
     * with an OR (RIMZA).
     *
     * It is reset during a cycle of channel 2 with CE18 (enable RIAP),
     * or at the end of a transfer on channel 2 (cpu fo. 114).
     */
    uint8_t RC02:1;

    /**
     * Asynchronous Channel 3 Cycle Request
     *
     * It is set with the OR of the cycle request triggers relative to
     * channel 3 (RA301=1) if the executing instruction is not over
     * (RIVAF=1) and additional performances of the GE-130 are enabled
     * (FUL4F=1).
     *
     * It is reset during a cycle of channel 3 with CE18 (enable RIAP),
     * also, it is reset when the SITE key is inserted (RAITI=1) during
     * a data transfer on channel 3 (RES36=1), or at the end of transfer
     * on channel 3 (PIC32=0) (cpu fo. 114).
     */
    uint8_t RC03:1;

    /**
     * Synchronous CPU Cycle Request
     *
     * Is conditioned by the signals ALTOF and RAM02.
     *
     * When the FF ALTOF is reset, the cycle requests from the CPU are
     * not serverd, therefore the internal calculation is stopped.
     * This counter consists of the FF RAMO and RAMI and counts with
     * the pulse TO10.
     */
    uint8_t RIA0:1;

    /**
     * Synchronous Channel 1 Cycle Request
     *
     * Transfered from RC01 at pulse TO00 (cpu fo. 114).
     */
    uint8_t RESI:1;

    /**
     * Synchronous Channel 2 Cycle Request
     *
     * Transfered from RC02 at pulse TO00 (cpu fo. 114).
     */
    uint8_t RIA2:1;

    /**
     * Synchronous Channel 3 Cycle Request
     *
     * Transfered from RC03 at pulse TO00 (cpu fo. 114).
     */
    uint8_t RIA3:1;

    /**
     * Future state
     *
     * Ad-hoc logic, at the end of the cycle contains the result
     * of the future state network.
     */
    uint8_t future_state;

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

    /**
     * Workaround for pulse TO50
     *
     * Currently we first run the common machine logic, then the
     * MSL states. However in certain cases (e.g. display state 00)
     * the common TO50 implementation is conditioned on the activation
     * of the MSL TO50...
     * So, until we figure out a better way of factoring the MSL, let's
     * store here the conditions for the common machine TO50,
     * and delay its excecution to a fake TO50-1 clock pulse.
     */
    struct {
        /* state 00 to 50: "if ci33 is absent, transfer NO in BO */
        uint8_t did_CI33:1;
    } TO50_conditions;
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

struct ge_peri {
    struct ge_peri *next;
    int (*init)(struct ge*, void*);
    int (*on_pulse)(struct ge*, void*);
    int (*on_clock)(struct ge*, void*);
    int (*deinit)(struct ge*, void*);
    void *ctx;
};

int ge_register_peri(struct ge *ge, struct ge_peri *p);

/**
 * Commit the future state
 *
 * Transfers the results of the future state network in the
 * various selectors. For now it's an ad hoc behaviour, not
 * described in detail in the currently available docs.
 */
void fsn_last_clock(struct ge *ge);

/**
 * The clock period name name
 *
 * Returns the string destribing the clock period
 */
const char *ge_clock_name(enum clock c);

#endif /* GE_H */
