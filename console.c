#include "console.h"
#include "ge.h"
#include "bit.h"
#include "log.h"
#include "signals.h"

void ge_fill_console_data(struct ge* ge, struct ge_console *console)
{
    /* maintenance panel lamps (cpu fo. 34) */

    console->lamps.RO = ge->rRO;
    console->lamps.SO = ge->rSO;
    console->lamps.FA = ge->ffFA & 0x0f;
    console->lamps.UR = ge->URPE;
    console->lamps.SA = ge->rSA;

    /* TODO: "selection of the four connectors"? */
    console->lamps.B = 0;

    /* operator panel lamps (cpu fo. 33) */

    console->lamps.HALT = ge->ALTO;
    console->lamps.OPERATOR_CALL = ge->ALAM;

    /* SWITCH 1 / SWITCH 2 lamps show the positions of the two program-readable
     * switches: lit when the switch reads logic 1 (the value that makes JS1 /
     * JS2 jump). (CPU[4] §3.3, fo.33) */
    console->lamps.SWITCH_1 = ge->JS1;
    console->lamps.SWITCH_2 = ge->JS2;

    /* LOAD1/LOAD2 selector lamps: the load-unit selector picks one of the two
     * install-time load connectors (ALOI=1 -> connector 2 = LOAD1, ALOI=0 ->
     * LOAD2). Exactly one is lit. (CPU[4] §3.3, fo.43) */
    console->lamps.LOAD_1 = ge->ALOI;
    console->lamps.LOAD_2 = !ge->ALOI;

    /* performance conditions (cpu fo. 31, 32) */

    console->lamps.ADD_reg = ge->rBO;

    console->lamps.OF = BIT(ge->ffFA, 4);
    console->lamps.NZ = BIT(ge->ffFA, 5);
    console->lamps.IM = BIT(ge->ffFA, 6);
    console->lamps.JE = ge->JE;

    console->lamps.I  = ge->INTE;
    console->lamps.C1 = PUC1(ge);
    console->lamps.C2 = ge->PUC2;
    console->lamps.C3 = ge->PUC3;

    console->lamps.MEM_CHECK = ge->mem_check;
    console->lamps.INV_ADD   = ge->inv_add;

    /* STEP BY STEP lamp reflects the PAPA (passo-passo) switch: when inserted
     * the CPU halts after each operation (fsn_last_clock re-arms ALTO), so each
     * START performs exactly one operation — the step-by-step mode used to key
     * data/instructions in from the panel. */
    console->lamps.STEP_BY_STEP = ge->console_switches.PAPA;

    console->lamps.OP_reg  = ge->rFO;

    console->rotary = ge->register_selector;
    console->switches = ge->console_switches;
}

void ge_set_console_switches(struct ge *ge, struct ge_console_switches *switches)
{
    ge_log(LOG_CONSOLE,
           "AM: %04x - switches: "
           "SITE: %d INCE: %d INAR: %d STOC: %d "
           "ACON: %d ACOV: %d RICI: %d PATE: %d PAPA: %d\n",
           switches->AM,
           switches->SITE, switches->INCE, switches->INAR, switches->STOC,
           switches->ACON, switches->ACOV, switches->RICI, switches->PATE, switches->PAPA);
    ge->console_switches = *switches;
}


void ge_set_console_rotary(struct ge *ge, enum ge_console_rotary rs)
{
    ge_log(LOG_CONSOLE, "setting rotary %d\n", rs);
    ge->register_selector = rs;
}
