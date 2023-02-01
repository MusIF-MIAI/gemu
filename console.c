#include "console.h"
#include "ge.h"

void ge_console_set_register_selector(
    struct ge *ge,
    enum ge_console_register_selector selector
) {
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
