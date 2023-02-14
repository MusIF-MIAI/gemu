#ifndef CONSOLE
#define CONSOLE

#include <stdint.h>

#define PACKED __attribute__((packed))

enum ge_console_rotary {
    RS_V4,
    RS_L3,
    RS_V3,
    RS_R1_L2,
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

struct PACKED ge_console_lamps {
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

    uint16_t DC_ALERT:1;       /* red */
    uint16_t POWER_OFF:1;      /* yellow */
    uint16_t STAND_BY:1;       /* blue */
    uint16_t POWER_ON:1;       /* yellow */
    uint16_t MAINTENANCE_ON:1; /* red */
    uint16_t MEM_CHECK:1;      /* red */
    uint16_t INV_ADD:1;        /* red */
    uint16_t SWITCH_1:1;       /* white */
    uint16_t SWITCH_2:1;       /* white */
    uint16_t STEP_BY_STEP:1;   /* white */
    uint16_t HALT:1;           /* white */
    uint16_t LOAD_1:1;         /* white */
    uint16_t LOAD_2:1;         /* white */
    uint16_t OPERATOR_CALL:1;  /* blue */
};

/**
 * Console switches
 *
 * Represents the switches and levers of the console's maintenance panel.
 */
struct PACKED ge_console_switches {
    /**
     * Step By Step execution
     *
     * Step by step eecution of microsequences of the cpu, after
     * each step is performed, without interfering with the transfers
     * of a peripheral unit.
     *
     * START starts the execution of a step.
     */
    uint16_t PAPA:1;

    /**
     * Stop after a cycle
     *
     * Stops the timing after every cycle of the delay line.
     * START starts the timing for a cycle.
     */
    uint16_t PATE:1;

    /**
     * Disables next status
     *
     * Disables the execution of commands loading the next status, allowing
     * to repeat its execution.
     */
    uint16_t RICI:1;

    /**
     * Stops on jump condition verified
     *
     * Stops the machine when a jump condition is verified at the end of
     * reading the jump instruction
     */
    uint16_t ACOV:1;

    /**
     * Stops on jump condition not verified
     *
     * Stops the machine when a jump condition is not verified at the end of
     * reading the jump instruction
     */
    uint16_t ACON:1;

    /**
     * Do not stop on memory error
     *
     * Inhibits the stopping should there be a check error reading from memory.
     * or the memory be addressed at a non-existing address.
     */
    uint16_t INAR:1;

    /**
     *
     */
    uint16_t STOC:1;

    /**
     * Do not error-correct external units input
     *
     * Inhibits the correction of the check bit for the character arriving from
     * external units.
     * When forcing in storage from the console, this switch causes AM08 to be
     * stored ad an odd parity bit.
     */
    uint16_t INCE:1;

    /**
     * Don't wait for external unit availability
     *
     * When it is inserted, the central processor will not wait for the
     * availabiltiy or the triggers from the external unit, allowing the
     * program to evolve normally.
     */
    uint16_t SITE:1;

    uint16_t lamps_on:1;
    uint16_t _pad_0:6;

    /**
     * Forcing bits
     *
     * The AM register forced by the console switches.
     */
    uint16_t AM;
};

struct PACKED ge_console_buttons {
    uint16_t AC_ON:1;
    uint16_t DC_ALERT:1;
    uint16_t POWER_ON:1;
    uint16_t MAINTENANCE_ON:1;
    uint16_t SWITCH_1:1;
    uint16_t SWITCH_2:1;
    uint16_t STEP_BY_STEP:1;
    uint16_t LOAD_1_2:1;
    uint16_t EMERGEN_OFF:1;
    uint16_t STANDBY:1;
    uint16_t _pad_0:1;
    uint16_t MEM_CHECK:1;
    uint16_t CLEAR:1;
    uint16_t LOAD:1;
    uint16_t HALT_START:1;
    uint16_t OPER_CALL:1;
};


struct PACKED ge_console {
    struct ge_console_lamps    lamps;
    struct ge_console_switches switches;
    struct ge_console_buttons  buttons;
    enum   ge_console_rotary   rotary;
};

struct ge;

void ge_fill_console_data(struct ge*, struct ge_console*);
void ge_set_console_switches(struct ge*, struct ge_console_switches*);
void ge_set_console_rotary(struct ge *, enum ge_console_rotary);

#endif
