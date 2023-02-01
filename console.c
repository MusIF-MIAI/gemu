#include "console.h"
#include "ge.h"

void ge_fill_console_data(struct ge* ge, struct ge_console *console)
{
    console->lamps.POWER_ON = ge->powered;
    console->lamps.HALT = ge->halted;
    console->lamps.OPERATOR_CALL = ge->operator_call;

    console->rotary = ge_console_get_rotary(ge);
}

enum ge_console_rotary ge_console_get_rotary(struct ge *ge)
{
    if (ge->AF10) return RS_V4;
    if (ge->AF20) return RS_L3;
    if (ge->AF21) return RS_L1;
    if (ge->AF30) return RS_V3;
    if (ge->AF31) return RS_V1;
    if (ge->AF32) return RS_NORM;
    if (ge->AF40) return RS_R1_L2;
    if (ge->AF41) return RS_V1_SCR;
    if (ge->AF42) return RS_PO;
    if (ge->AF43) return RS_SO;
    if (ge->AF50) return RS_V2;
    if (ge->AF51) return RS_V1_LETT;
    if (ge->AF52) return RS_FI_UR;
    if (ge->AF53) return RS_FO;

    return RS_NORM;
}

void ge_console_set_rotary(struct ge *ge, enum ge_console_rotary selector)
{
    ge->AF10 = selector == RS_V4;
    ge->AF20 = selector == RS_L3;
    ge->AF21 = selector == RS_L1;
    ge->AF30 = selector == RS_V3;
    ge->AF31 = selector == RS_V1;
    ge->AF32 = selector == RS_NORM;
    ge->AF40 = selector == RS_R1_L2;
    ge->AF41 = selector == RS_V1_SCR;
    ge->AF42 = selector == RS_PO;
    ge->AF43 = selector == RS_SO;
    ge->AF50 = selector == RS_V2;
    ge->AF51 = selector == RS_V1_LETT;
    ge->AF52 = selector == RS_FI_UR;
    ge->AF53 = selector == RS_FO;
}
