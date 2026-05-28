#include "alu_cc.h"
#include "bit.h"

void alu_set_cc(struct ge *ge, uint8_t cc)
{
    /* cc bit1 -> ffFI bit4 (high), cc bit0 -> ffFI bit5 (low) */
    if (cc & 0x2)
        SET_BIT(ge->ffFI, 4);
    else
        RESET_BIT(ge->ffFI, 4);

    if (cc & 0x1)
        SET_BIT(ge->ffFI, 5);
    else
        RESET_BIT(ge->ffFI, 5);
}

uint8_t alu_get_cc(struct ge *ge)
{
    return (uint8_t)((BIT(ge->ffFI, 4) << 1) | BIT(ge->ffFI, 5));
}
