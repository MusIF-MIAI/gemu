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
        uint16_t RO:9,
                _pad0:3,
                UR:1,
                _pad1:3;    /* RO 9 LSbits  + UR at bit 12 */

        uint16_t SO:8,
                 _pad2:4,
                 FA:4;      /* SO 8 bits  + FA bits 12-15 */

        uint16_t SA:8,
                 _pad3:4,
                 B:4;       /* SA 8 bits  + B bits 12-15 */

        uint16_t ADD_reg;   /* 4 nibbles ADD REG - front panel */
        uint16_t OP_reg:8,
                 C3:1,
                 C2:1,
                 C1:1,
                 I:1,
                 JE:1,
                 IM:1,
                 NZ:1,
                 OF:1;
        uint16_t
            LP_DC_ALERT:1,       /* red */
            LP_POWER_OFF:1,      /* yellow */
            LP_STAND_BY:1,       /* blue */
            LP_POWER_ON:1,       /* yellow */
            LP_MAINTENANCE_ON:1, /* red */
            LP_MEM_CHECK:1,      /* red */
            LP_INV_ADD:1,        /* red */
            LP_SWITCH_1:1,       /* white */
            LP_SWITCH_2:1,       /* white */
            LP_STEP_BY_STEP:1,   /* white */
            LP_HALT:1,           /* white */
            LP_LOAD_1:1,         /* white */
            LP_LOAD_2:1,         /* white */
            LP_OPERATOR_CALL:1;  /* blue */
    } lamps;

    struct __attribute__((packed)) console_switch {
        uint16_t PAPA:1,
                 PATE:1,
                 RICI:1,
                 ACOV:1,
                 ACON:1,
                 INAR:1,
                 INCE:1,
                 SITE:1,
                 lamps_on:1,
                 _padding:6;
        uint16_t AM;
    } switches;

    struct __attribute__((packed)) console_button {
        uint16_t
            B_AC_ON:1,
            B_DC_ALERT:1,
            B_POWER_ON:1,
            B_MAINTENANCE_ON:1,
            B_SWITCH_1:1,
            B_SWITCH_2:1,
            B_STEP_BY_STEP:1,
            B_LOAD_1_2:1,
            B_EMERGEN_OFF:1,
            B_STANDBY:1,
            __padding:1,
            B_MEM_CHECK:1,
            B_CLEAR:1,
            B_LOAD:1,
            B_HALT_START:1,
            B_OPER_CALL:1;
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
 * @ALTO: FF Alto
 * @AVER: FF AVER
 * @ADIR: FF ADIR (disable step-by-step (pag. 97))
 * @RINT
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
    struct pulse_event *on_pulse[END_OF_STATUS];

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
    uint8_t ALTO:1;
    uint8_t ALAM:1;
    uint8_t AVER:1;
    uint8_t ADIR:1;
    uint8_t RINT:1;

    /* Faults: TODO (pp. 139-141) */
    uint8_t rFA;

    struct ge_console console;
    struct ge_counting_network counting_network;

    /* Memory */
    uint8_t mem[MEM_SIZE];


    int step_by_step:1; /* XXX: replace with signal name */
    int operator_call:1; /* XXX: replace with signal name */

    int JS1:1; /* console jump condition */
    int JS2:1; /* console jump condition */
    int JE:1; /* JE/AVER jump instruction exectuted */
    int INTE:1; /* interruption present */
    int PUC1:1; /* Channel 1 busy or CPU waiting */
    int PUC2:1; /* Channel 2 busy */
    int PUC3:1; /* Channel 3 busy */

    int URPE:1; /*  */
    int URPU:1; /*  */

    struct ge_peri *peri;
};

/// Initialize the emulator
int ge_init(struct ge *ge);

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

/**
 */
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
