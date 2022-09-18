#include "console.h"
#include "ge.h"

void console_set_switch(struct ge *ge, enum console_switches sw, int val)
{
    switch (sw) {
        case SW_LAMPS:
            ge->LAMPS = val;
            break;
        case SW_PAPA:
            ge->PAPA = val;
            break;
        case SW_PATE:
            ge->PATE = val;
            break;
        case SW_RICI:
            ge->RICI = val;
            break;
        case SW_ACOV:
            ge->ACOV = val;
            break;
        case SW_ACON:
            ge->ACON = val;
            break;
        case SW_INAR:
            ge->INAR = val;
            break;
        case SW_INCE:
            ge->INCE = val;
            break;
        case SW_SITE:
            ge->SITE = val;
            break;
        case SW_AM:
            ge->AM = val;
            break;
        case SW_ROTARY:
            ge->register_rotary = val;
            break;
    }
}

uint32_t console_get_lamp(struct ge *ge, enum console_lamps lp)
{
    switch (lp) {
        case LP_DC_ALERT:
            return 0;
        case LP_POWER_OFF:
            return 0;
        case LP_STAND_BY:
            return 0;
        case LP_POWER_ON:
            return 1;
        case LP_MAINTENANCE_ON:
            return ge->register_rotary != RS_NORM;
        case LP_MEM_CHECK:
            return 0;
        case LP_INV_ADD:
            return 0;
        case LP_SWITCH_1:
            return ge->JS1;
        case LP_SWITCH_2:
            return ge->JS2;
        case LP_STEP_BY_STEP:
            return ge->step_by_step;
        case LP_HALT:
            return ge->halted;
        case LP_LOAD_1:
            return 0;
        case LP_LOAD_2:
            return 0;
        case LP_OPERATOR_CALL:
            return ge->operator_call;
        case LP_ADDR_REG:
            return ge->rPO;
        case LP_PROG_COND:
            return ((ge->rFA & (1 << 4) >> 4) << 4) |
                   ((ge->rFA & (1 << 5) >> 5) << 3) |
                   ((ge->rFA & (1 << 6) >> 6) << 2) |
                   ge->JE;
        case LP_EXT_COND:
            return (ge->INTE << 4) |
                   (ge->PUC1 << 3) |
                   (ge->PUC2 << 2) |
                   (ge->PUC3 << 1);
        case LP_OP_REG:
            return ge->rFO;
        case LP_RO:
            return ge->rRO;
        case LP_SO:
            return ge->rSO;
        case LP_SA:
            return ge->rSA;
        case LP_B:
            return ge->rBO;
        case LP_FA:
            return ge->rFA;
        case LP_UR:
            return ge->URPE;
    }
}
