#ifndef GE_H
#define GE_H

#include <stdint.h>

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

enum register_switch {
    RS_V4,
    RS_L3,
    RS_V3,
    RS_RI_L2,
    RS_V2,
    RS_L1,
    RS_V1,
    RS_V1_SCR,
    RS_V1_LETT,
    RS_NORM,
    RS_PO,
    RS_FI_UR,
    RS_SO,
    RS_FO,
};

struct __attribute__((packed)) ge_console {
    struct __attribute__((packed)) console_lamp {
        uint16_t RO:9;
        uint16_t _pad0:3;
        uint16_t UR:1;
        uint16_t _pad1:3;    /* RO 9 LSbits  + UR at bit 12 */

        uint16_t SO:8;
        uint16_t _pad2:4;
        uint16_t FA:4;      /* SO 8 bits  + FA bits 12-15 */

        uint16_t SA:8;
        uint16_t _pad3:4;
        uint16_t B:4;       /* SA 8 bits  + B bits 12-15 */

        uint16_t ADD_reg;   /* 4 nibbles ADD REG - front panel */
        uint16_t OP_reg:8;
        uint16_t C3:1;
        uint16_t C2:1;
        uint16_t C1:1;
        uint16_t I:1;
        uint16_t JE:1;
        uint16_t IM:1;
        uint16_t NZ:1;
        uint16_t OF:1;

        uint16_t LP_DC_ALERT:1;       /* red */
        uint16_t LP_POWER_OFF:1;      /* yellow */
        uint16_t LP_STAND_BY:1;       /* blue */
        uint16_t LP_POWER_ON:1;       /* yellow */
        uint16_t LP_MAINTENANCE_ON:1; /* red */
        uint16_t LP_MEM_CHECK:1;      /* red */
        uint16_t LP_INV_ADD:1;        /* red */
        uint16_t LP_SWITCH_1:1;       /* white */
        uint16_t LP_SWITCH_2:1;       /* white */
        uint16_t LP_STEP_BY_STEP:1;   /* white */
        uint16_t LP_HALT:1;           /* white */
        uint16_t LP_LOAD_1:1;         /* white */
        uint16_t LP_LOAD_2:1;         /* white */
        uint16_t LP_OPERATOR_CALL:1;  /* blue */
    } lamps;

    struct __attribute__((packed)) console_switch {
        uint16_t PAPA:1;
        uint16_t PATE:1;
        uint16_t RICI:1;
        uint16_t ACOV:1;
        uint16_t ACON:1;
        uint16_t INAR:1;
        uint16_t INCE:1;
        uint16_t SITE:1;
        uint16_t lamps_on:1;
        uint16_t _pad0:6;
        uint16_t AM;
    } switches;

    struct __attribute__((packed)) console_button {
        uint16_t B_AC_ON:1;
        uint16_t B_DC_ALERT:1;
        uint16_t B_POWER_ON:1;
        uint16_t B_MAINTENANCE_ON:1;
        uint16_t B_SWITCH_1:1;
        uint16_t B_SWITCH_2:1;
        uint16_t B_STEP_BY_STEP:1;
        uint16_t B_LOAD_1_2:1;
        uint16_t B_EMERGEN_OFF:1;
        uint16_t B_STANDBY:1;
        uint16_t _pad0:1;
        uint16_t B_MEM_CHECK:1;
        uint16_t B_CLEAR:1;
        uint16_t B_LOAD:1;
        uint16_t B_HALT_START:1;
        uint16_t B_OPER_CALL:1;
    } buttons;

    uint8_t rotary;
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
    uint64_t ticks;
    uint8_t halted;
    uint8_t realtime;

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
     * It is used to establish the sequence for all the phases of program loading,
     * fetching (phase alpha), executing (phase beta)
     */
    uint8_t rSO;

    /**
     * Peripheral unit sequencer
     *
     * 4-bit sequencer used for data xechange with peripheral units through channel 2
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
     * Special conditions register 1
     *
     * 7 Flip-Flops containing special conditions which occur during the performance
     * of an instruction. Loaded from #ffFI in T010
     */
    uint8_t ffFA;

    uint8_t rFA;    ///< Faults (pp. 139-141)

    /**
     * Program Loading
     *
     * Set by pressing the "LOAD" button of the console, and it is reset by pressing
     * "CLEAR", or with the command CI39 (in the alpha phase of the E0 state).
     */
    uint8_t AINI:1;
    uint8_t ALTO:1;
    uint8_t ALAM:1;
    uint8_t AVER:1;

    uint8_t ADIR:1; ///< Disable step-by-step (pag. 97)
    uint8_t RINT:1;

    uint8_t JS1:1;  ///< Console jump condition 1
    uint8_t JS2:1;  ///< Console jump condition 2
    uint8_t JE:1;   ///< JE/AVER jump instruction exectuted
    uint8_t INTE:1; ///< Interruption present
    uint8_t PUC1:1; ///< Channel 1 busy or CPU waiting
    uint8_t PUC2:1; ///< Channel 2 busy
    uint8_t PUC3:1; ///< Channel 3 busy

    uint8_t URPE:1;
    uint8_t URPU:1;

    uint8_t step_by_step:1;  ///< Step by step execution @todo replace with signal name
    uint8_t operator_call:1; ///< Operator call @todo replace with signal name

    uint8_t mem[MEM_SIZE]; ///< The memory of the emulated system

    struct ge_console console;
    struct ge_counting_network counting_network;
    struct ge_peri *peri;
};

/// Initialize the emulator
int ge_init(struct ge *ge);

/// Deinitialize the emulator
int ge_deinit(struct ge *ge);

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
