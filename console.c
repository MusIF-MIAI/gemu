#include "console.h"
#include "ge.h"

void ge_fill_console_data(struct ge* ge, struct ge_console *console)
{
    console->lamps.POWER_ON = ge->powered;
    console->lamps.HALT = ge->halted;
    console->lamps.OPERATOR_CALL = ge->ALAM;

    console->rotary = ge->register_selector;
    console->switches = ge->console_switches;
}
