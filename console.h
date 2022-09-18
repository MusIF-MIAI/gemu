#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdint.h>

struct ge;

enum generic_switch {
    SW_OFF,
    SW_ON,
};

enum lamps_switch {
    LW_OFF,
    LW_ON,
    LW_DIAG,
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

enum console_switches {
    /* maintenance panel */
    SW_LAMPS,
    SW_PAPA,
    SW_PATE,
    SW_RICI,
    SW_ACOV,
    SW_ACON,
    SW_INAR,
    SW_INCE,
    SW_SITE,
    SW_AM,
    SW_ROTARY,
};

enum console_lamps {
    /* operator panel */
    LP_DC_ALERT,       /* red */
    LP_POWER_OFF,      /* yellow */
    LP_STAND_BY,       /* blue */
    LP_POWER_ON,       /* yellow */
    LP_MAINTENANCE_ON, /* red */
    LP_MEM_CHECK,      /* red */
    LP_INV_ADD,        /* red */
    LP_SWITCH_1,       /* white */
    LP_SWITCH_2,       /* white */
    LP_STEP_BY_STEP,   /* white */
    LP_HALT,           /* white */
    LP_LOAD_1,         /* white */
    LP_LOAD_2,         /* white */
    LP_OPERATOR_CALL,  /* blue */

    LP_ADDR_REG,       /* 16 bit row */
    LP_PROG_COND,      /* 4 bit row (OF, NZ, IM, HE) */
    LP_EXT_COND,       /* 4 bit row, (I, C1, C2, C3) Interruption, Channel {1, 2, 3} busy  */
    LP_OP_REG,         /* 8 bit row, register FO */

    /* mainatenance panel */
    LP_RO,             /* 9 bit row */
    LP_SO,             /* 8 bit row */
    LP_SA,             /* 8 bit row */
    LP_B,              /* 4 bit row */
    LP_FA,             /* 4 bit row */
    LP_UR,
};

void console_set_switch(struct ge *, enum console_switches, int);
uint32_t console_get_lamp(struct ge *, enum console_lamps);

#endif /* CONSOLE_H */
