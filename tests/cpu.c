#include "utest.h"
#include "../ge.h"
#include "../log.h"
#include "../signals.h"   /* ge_mem_in_bounds / ge_memory_capacity_k (ch.080) */

#include <string.h>

/** diag fo. 82 */
UTEST(cpu_isolation, test_k)
{
    struct ge g;
    struct ge_console c = { 0 };
    struct ge_console_switches s = { 0 };

    ge_log(LOG_CONSOLE, "CLEAR\n");
    ge_init(&g);
    ge_clear(&g);
    ge_run_cycle(&g);

    ge_log(LOG_CONSOLE, "SET V1 SCR, AM = ff, INAR\n");
    ge_set_console_rotary(&g, RS_V1_SCR);
    s.AM = 0x00ff;
    s.INAR = 1;
    ge_set_console_switches(&g, &s);
    ge_run_cycle(&g);

    ge_log(LOG_CONSOLE, "START\n");
    ge_start(&g);
    ge_run_cycle(&g);

    ASSERT_EQ(g.mem[0], 0xff);

    ge_log(LOG_CONSOLE, "SET PAPA\n");
    s.PAPA = 1;
    ge_set_console_switches(&g, &s);
    ge_run_cycle(&g);

    ge_log(LOG_CONSOLE, "RESET PAPA, INAR\n");
    s.PAPA = 0;
    s.INAR = 0;
    ge_set_console_switches(&g, &s);
    ge_run_cycle(&g);

    ge_log(LOG_CONSOLE, "CLEAR\n");
    ge_clear(&g);
    ge_run_cycle(&g);

    ge_log(LOG_CONSOLE, "SET V1, AM = 00\n");
    ge_set_console_rotary(&g, RS_V1);
    s.AM = 0x0000;
    ge_set_console_switches(&g, &s);
    ge_run_cycle(&g);

    ge_log(LOG_CONSOLE, "START\n");
    ge_start(&g);
    ge_run_cycle(&g);

    ASSERT_TRUE(g.rV1 == 0x00);

    ge_log(LOG_CONSOLE, "SET V1 LETT\n");
    ge_set_console_rotary(&g, RS_V1_LETT);
    ge_run_cycle(&g);

    ge_log(LOG_CONSOLE, "START\n");
    ge_start(&g);
    ge_run_cycle(&g);
    ge_fill_console_data(&g, &c);

    ge_log(LOG_CONSOLE, "RO == FF?!?\n");

    ASSERT_TRUE(c.lamps.RO == 0x0ff);
    ASSERT_TRUE(c.lamps.MEM_CHECK == 0);
}

/*
 * Light OPER CALL by keying a LON instruction (opcode 0x02, 2nd char 0x80)
 * into the CPU registers from the maintenance panel and single-stepping it
 * through the instruction phases — the exact real-GE-120 front-panel
 * procedure (CPU[4] §4 "Maintenance Panel", dwg 30004122 fo. 35-37):
 *
 *   CLEAR; insert PAPA (passo-passo / step-by-step)
 *   rotary SO  : force 0xE2  -> sequencer set to the alpha phase
 *   rotary PO  : force 0x00  -> program counter (SO preserved across the force)
 *   NORM, START: step alpha (0xE2) -> operand-fetch phase E0
 *   rotary FO  : force 0x02  -> the LON opcode (SO held at E0)
 *   NORM, START: E0 decodes a 2-byte op -> beta phase 0x64
 *   rotary L1  : force 0x80  -> LON 2nd char (SO held at 0x64)
 *   NORM, START: beta executes LON -> CI87 sets ALAM -> OPER CALL on
 *
 * This works because a forcing cycle preserves the program sequencer SO
 * (fsn_last_clock); only forcing SO itself (rotary pos 13) changes it.
 */
static void cons_force(struct ge *g, struct ge_console_switches *s, int rot, int am)
{
    ge_set_console_rotary(g, rot);
    s->AM = am;
    s->INAR = 1;
    ge_set_console_switches(g, s);
    ge_run_cycle(g);
    ge_start(g);
    ge_run_cycle(g);
}

static void cons_step(struct ge *g, struct ge_console_switches *s)
{
    s->INAR = 0;
    ge_set_console_rotary(g, RS_NORM);
    ge_set_console_switches(g, s);
    ge_start(g);
    ge_run_cycle(g);
}

UTEST(cpu_isolation, oper_call_by_register_forcing)
{
    struct ge g;
    struct ge_console c = { 0 };
    struct ge_console_switches s = { 0 };

    ge_init(&g);
    ge_clear(&g);
    ge_run_cycle(&g);

    s.PAPA = 1;                       /* step-by-step (passo-passo) */
    ge_set_console_switches(&g, &s);

    cons_force(&g, &s, RS_SO, 0xE2);  /* sequencer -> alpha phase */
    cons_force(&g, &s, RS_PO, 0x00);  /* PO <- 0 (SO preserved) */
    ASSERT_EQ((int)g.rSO, 0xE2);      /* the forced alpha state held */

    cons_step(&g, &s);                /* alpha -> E0 (instruction fetch) */
    ASSERT_EQ((int)g.rSO, 0xE0);

    cons_force(&g, &s, RS_FO, 0x02);  /* LON opcode -> FO (SO held at E0) */
    ASSERT_EQ((int)g.rSO, 0xE0);

    cons_step(&g, &s);                /* E0 -> beta 0x64 (2-byte op) */
    ASSERT_EQ((int)g.rSO, 0x64);
    ASSERT_EQ((int)g.rFO, 0x02);

    cons_force(&g, &s, RS_L1, 0x80);  /* LON 2nd char -> L1 (SO held at 0x64) */
    ASSERT_EQ((int)g.rSO, 0x64);

    /* final step: beta executes LON */
    s.INAR = 0;
    ge_set_console_rotary(&g, RS_NORM);
    ge_set_console_switches(&g, &s);
    ge_start(&g);
    int lit = 0;
    for (int i = 0; i < 25; i++) {
        ge_run_cycle(&g);
        if (g.ALAM) {
            lit = 1;
            break;
        }
    }

    ASSERT_EQ(lit, 1);                            /* LON executed -> ALAM set */
    ge_fill_console_data(&g, &c);
    ASSERT_EQ((int)c.lamps.OPERATOR_CALL, 1);     /* OPER CALL lamp on */
    /* The whole walk above was driven by the maintenance PAPA switch, and PAPA
     * has no lamp: STEP BY STEP belongs to the operator panel's own switch
     * (ASIN), a separate circuit. This used to assert 1, back when gemu treated
     * the two as one thing. */
    ASSERT_EQ((int)c.lamps.STEP_BY_STEP, 0);
}

/*
 * SWITCH 1 / SWITCH 2 lamps follow the two program-readable switches: lit when
 * the switch reads logic 1 (the value that makes JS1 / JS2 jump).
 * CPU[4] §3.3, fo.33.
 */
UTEST(console_fidelity, switch_lamps)
{
    struct ge g;
    struct ge_console c = { 0 };

    ge_init(&g);
    ge_clear(&g);

    g.JS1 = 1;
    g.JS2 = 0;
    ge_fill_console_data(&g, &c);
    ASSERT_EQ((int)c.lamps.SWITCH_1, 1);
    ASSERT_EQ((int)c.lamps.SWITCH_2, 0);

    g.JS1 = 0;
    g.JS2 = 1;
    ge_fill_console_data(&g, &c);
    ASSERT_EQ((int)c.lamps.SWITCH_1, 0);
    ASSERT_EQ((int)c.lamps.SWITCH_2, 1);
}

/*
 * MAINT ON follows the panel, not the program: lit when a maintenance switch is
 * inserted OR the register selector is off NORM, and only while the machine is
 * stopped. Observed on the restored machine, 2026-07-29.
 */
UTEST(console_fidelity, maint_on_lamp)
{
    struct ge g;
    struct ge_console c = { 0 };
    struct ge_console_switches s = { 0 };

    ge_init(&g);
    ge_clear(&g);                       /* stopped: ALTO = 1 */

    /* Untouched panel, rotary at NORM, machine stopped: dark. */
    ge_set_console_rotary(&g, RS_NORM);
    ge_set_console_switches(&g, &s);
    ge_fill_console_data(&g, &c);
    ASSERT_EQ((int)c.lamps.MAINTENANCE_ON, 0);

    /* One switch inserted is enough. */
    s.PAPA = 1;
    ge_set_console_switches(&g, &s);
    ge_fill_console_data(&g, &c);
    ASSERT_EQ((int)c.lamps.MAINTENANCE_ON, 1);

    /* ... and so is the rotary alone, with every switch out. */
    s.PAPA = 0;
    ge_set_console_switches(&g, &s);
    ge_set_console_rotary(&g, RS_V1_SCR);
    ge_fill_console_data(&g, &c);
    ASSERT_EQ((int)c.lamps.MAINTENANCE_ON, 1);

    /* But not while the machine is running. */
    ge_start(&g);
    ge_fill_console_data(&g, &c);
    ASSERT_EQ((int)c.lamps.MAINTENANCE_ON, 0);

    /* Stopping it again with the panel still off NORM lights it back up. */
    g.ALTO = 1;
    ge_fill_console_data(&g, &c);
    ASSERT_EQ((int)c.lamps.MAINTENANCE_ON, 1);

    /* The AM forcing toggles are switches on that panel too, and count. */
    ge_set_console_rotary(&g, RS_NORM);
    memset(&s, 0, sizeof(s));
    s.AM = 0x0001;
    ge_set_console_switches(&g, &s);
    ge_fill_console_data(&g, &c);
    ASSERT_EQ((int)c.lamps.MAINTENANCE_ON, 1);

    s.AM = 0x8000;
    ge_set_console_switches(&g, &s);
    ge_fill_console_data(&g, &c);
    ASSERT_EQ((int)c.lamps.MAINTENANCE_ON, 1);

    s.AM = 0;
    ge_set_console_switches(&g, &s);
    ge_fill_console_data(&g, &c);
    ASSERT_EQ((int)c.lamps.MAINTENANCE_ON, 0);

    /* Every maintenance switch counts, one at a time. */
    ge_set_console_rotary(&g, RS_NORM);
    {
        struct ge_console_switches each[9] = { {0} };
        each[0].PATE = 1; each[1].RICI = 1; each[2].ACOV = 1;
        each[3].ACON = 1; each[4].INAR = 1; each[5].STOC = 1;
        each[6].INCE = 1; each[7].SITE = 1; each[8].PAPA = 1;
        for (int i = 0; i < 9; i++) {
            ge_set_console_switches(&g, &each[i]);
            ge_fill_console_data(&g, &c);
            ASSERT_EQ((int)c.lamps.MAINTENANCE_ON, 1);
        }
    }
}

/*
 * LAMPS CHECK is a bulb test: held, every console lamp lights whatever the
 * machine is doing, and nothing in the CPU is disturbed. CPU[4] §3.2.
 */
UTEST(console_fidelity, lamps_check_lights_everything)
{
    struct ge g;
    struct ge_console c = { 0 };

    ge_init(&g);
    ge_clear(&g);

    /* Cold panel: most lamps dark. */
    ge_fill_console_data(&g, &c);
    ASSERT_EQ((int)c.lamps.OPERATOR_CALL, 0);
    ASSERT_EQ((int)c.lamps.MEM_CHECK, 0);
    ASSERT_EQ((int)c.lamps.SWITCH_1, 0);
    ASSERT_EQ((int)c.lamps.RO, 0);

    /* Held: everything on, including the register lamps. */
    g.lamps_test = 1;
    ge_fill_console_data(&g, &c);
    ASSERT_EQ((int)c.lamps.OPERATOR_CALL, 1);
    ASSERT_EQ((int)c.lamps.MEM_CHECK, 1);
    ASSERT_EQ((int)c.lamps.SWITCH_1, 1);
    ASSERT_EQ((int)c.lamps.MAINTENANCE_ON, 1);
    ASSERT_EQ((int)c.lamps.HALT, 1);
    ASSERT_EQ((int)c.lamps.RO, 0x1FF);       /* all nine RO lamps */
    ASSERT_EQ((int)c.lamps.SO, 0xFF);
    ASSERT_EQ((int)c.lamps.SA, 0xFF);
    ASSERT_EQ((int)c.lamps.ADD_reg, 0xFFFF);

    /* Released: the real states come back, and the CPU never noticed. */
    g.lamps_test = 0;
    ge_fill_console_data(&g, &c);
    ASSERT_EQ((int)c.lamps.OPERATOR_CALL, 0);
    ASSERT_EQ((int)c.lamps.MEM_CHECK, 0);
    ASSERT_EQ((int)c.lamps.RO, 0);
    ASSERT_EQ((int)g.ALTO, 1);               /* still just stopped by CLEAR */
}

/*
 * STEP-BY-STEP (operator panel, ASIN) and PAPA (maintenance panel) are two
 * independent circuits, CPU[4] fo.115:
 *
 *   ASIN -> CI891 at E2/E3 of alpha: stops at each INSTRUCTION, gated by the
 *           program (INS sets ADIR; ENS/CLEAR clear it) with STOC overriding.
 *   PAPA -> ALS71 after a CPU work cycle: stops after each MICROSEQUENCE, and
 *           is not gated by the program at all.
 *
 * Only ASIN has a lamp.
 */
UTEST(console_fidelity, step_by_step_and_papa_are_separate)
{
    struct ge g;
    struct ge_console c = { 0 };
    struct ge_console_switches s = { 0 };

    ge_init(&g);
    ge_clear(&g);
    g.register_selector = RS_NORM;
    g.RC00 = 1;
    g.RIA0 = 1;

    /* --- PAPA: steps microsequences, and the program cannot stop it. --- */
    s.PAPA = 1;
    g.console_switches = s;

    g.ADIR = 0; g.ALTO = 0;
    fsn_last_clock(&g);
    ASSERT_EQ((int)g.ALTO, 1);

    /* INS (ADIR) inhibits STEP-BY-STEP, never PAPA: PAPA still halts. */
    g.ADIR = 1; g.ALTO = 0;
    fsn_last_clock(&g);
    ASSERT_EQ((int)g.ALTO, 1);

    /* ... and PAPA lights no lamp. */
    ge_fill_console_data(&g, &c);
    ASSERT_EQ((int)c.lamps.STEP_BY_STEP, 0);

    /* --- ASIN: stops at each instruction, and the program CAN inhibit it. --- */
    s.PAPA = 0;
    g.console_switches = s;
    g.ASIN = 1;

    /* It is the operator switch that owns the lamp. */
    ge_fill_console_data(&g, &c);
    ASSERT_EQ((int)c.lamps.STEP_BY_STEP, 1);

    /* It does not come through the microsequence path at all. */
    g.ADIR = 0; g.ALTO = 0;
    fsn_last_clock(&g);
    ASSERT_EQ((int)g.ALTO, 0);

}

/* Run a short NOP2 program with the given panel state and report how many
 * cycles RAN before the machine stopped (so 1 = it stopped on the first cycle,
 * which is what "interrupted at the beginning of the instruction" looks like),
 * or -1 if it ran the budget out. */
static long cycles_to_halt(int asin, int adir, int stoc, int papa)
{
    /* NOP2 NOP2 NOP2 HLT */
    static uint8_t prog[] = { 0x07, 0x00, 0x07, 0x00, 0x07, 0x00, 0x0A, 0x00 };
    struct ge g;
    struct ge_console_switches s = { 0 };
    long i;

    ge_init(&g);
    ge_log_set_active_types(0);
    ge_clear(&g);
    ge_load_image(&g, prog, sizeof(prog), 0);
    s.STOC = stoc;
    s.PAPA = papa;
    ge_set_console_switches(&g, &s);
    ge_set_console_rotary(&g, RS_NORM);
    g.ASIN = asin;
    g.ADIR = adir;
    ge_start(&g);
    ge_enter(&g, 0x0000);

    for (i = 0; i < 400; i++) {
        if (ge_run_cycle(&g) != 0)
            break;
        if (ge_halted(&g))
            return i + 1;
    }
    return -1;
}

/*
 * The same two circuits, seen from outside: how soon does the machine stop?
 *
 * Free-running it reaches the HLT at the end of the program. With STEP-BY-STEP
 * inserted it stops much sooner, at the first instruction -- unless the program
 * has inhibited it with INS, which STOC then overrides. PAPA stops sooner still
 * (a microsequence is shorter than an instruction) and no INS can stop it.
 */
UTEST(console_fidelity, step_by_step_stops_earlier_than_the_program_end)
{
    long free_run = cycles_to_halt(0, 0, 0, 0);
    long stepping = cycles_to_halt(1, 0, 0, 0);
    long inhibited = cycles_to_halt(1, 1, 0, 0);
    long overridden = cycles_to_halt(1, 1, 1, 0);
    long papa = cycles_to_halt(0, 0, 0, 1);
    long papa_inhibited = cycles_to_halt(0, 1, 0, 1);

    ASSERT_GT(free_run, 0);                 /* reaches its own HLT       */
    ASSERT_GT(stepping, 0);
    ASSERT_LT(stepping, free_run);          /* STEP-BY-STEP stops sooner */

    ASSERT_EQ(inhibited, free_run);         /* INS inhibits it entirely  */
    ASSERT_EQ(overridden, stepping);        /* STOC brings it back       */

    ASSERT_GT(papa, 0);
    ASSERT_LE(papa, stepping);              /* a microsequence is shorter */
    ASSERT_EQ(papa_inhibited, papa);        /* and INS cannot touch PAPA  */
}

/*
 * PATE stops the timing after every delay-line cycle — finer than PAPA, and
 * ungated by the CPU/channel cycle. With PAPA off and the rotary in NORM the
 * machine free-runs (no halt); inserting PATE halts it after a single cycle.
 * CPU[4] §4, fo.35.
 */
UTEST(console_fidelity, pate_single_cycle)
{
    struct ge g;
    struct ge_console_switches s = { 0 };

    ge_init(&g);
    ge_clear(&g);
    g.register_selector = RS_NORM;
    g.RC00 = 1;
    g.RIA0 = 1;

    /* baseline: free-running (no PAPA, NORM rotary) -> no halt */
    g.console_switches = s;
    g.ALTO = 0;
    fsn_last_clock(&g);
    ASSERT_EQ((int)g.ALTO, 0);

    /* PATE halts after one delay-line cycle */
    s.PATE = 1;
    g.console_switches = s;
    g.ALTO = 0;
    fsn_last_clock(&g);
    ASSERT_EQ((int)g.ALTO, 1);
}

/*
 * INCE check-bit forcing (CPU[4] §4.2, fo.36-37): with the rotary in V1 SCR
 * (storage forcing) and INCE inserted, AM08 is stored as the memory parity bit
 * instead of generated odd parity — so the operator can key in a wrong check
 * bit to exercise MEM CHECK. AM08=1 is wrong for 0x01 (popcount 1 is odd ->
 * correct parity 0), so reading the byte back raises MEM CHECK.
 */
UTEST(console_fidelity, ince_forces_check_bit)
{
    struct ge g;
    struct ge_console c = { 0 };
    struct ge_console_switches s = { 0 };

    ge_init(&g);
    ge_clear(&g);
    ge_run_cycle(&g);

    /* force 0x01 into mem[0] with a deliberately wrong check bit (AM08 = 1) */
    ge_set_console_rotary(&g, RS_V1_SCR);
    s.AM   = 0x0101;   /* AM07-00 = 0x01 (data); AM08 = 1 (forced parity) */
    s.INCE = 1;
    s.INAR = 1;        /* inhibit the fault stop while keying in */
    ge_set_console_switches(&g, &s);
    ge_run_cycle(&g);
    ge_start(&g);
    ge_run_cycle(&g);

    ASSERT_EQ((int)g.mem[0], 0x01);
    ASSERT_EQ((int)g.mem_parity[0], 1);   /* AM08, not odd_parity(0x01) = 0 */

    /* point V1 at address 0, then read it back through V1 LETT */
    ge_clear(&g);
    ge_run_cycle(&g);
    ge_set_console_rotary(&g, RS_V1);
    s.AM   = 0x0000;
    s.INCE = 0;
    s.INAR = 0;
    ge_set_console_switches(&g, &s);
    ge_start(&g);
    ge_run_cycle(&g);

    ge_set_console_rotary(&g, RS_V1_LETT);
    ge_run_cycle(&g);
    ge_start(&g);
    ge_run_cycle(&g);

    ge_fill_console_data(&g, &c);
    ASSERT_EQ((int)c.lamps.RO, 0x01);
    ASSERT_EQ((int)c.lamps.MEM_CHECK, 1);   /* wrong check bit detected on read */
}


/*
 * CLEAR clears the error conditions.
 *
 * "Stops everything in the subsystem, clears all error conditions, presets CPU
 * + peripherals to a defined state. Required after MEM CHECK and after
 * power-on." (CPU[4] §3.3, fo.31-33.) The two fault latches the panel shows are
 * MEM CHECK (parity) and INV ADD (address past installed core); nothing else in
 * the machine takes them down, so before this they stayed lit for a fault the
 * operator had already acknowledged with the key the manual names.
 */
UTEST(console_fidelity, clear_clears_the_fault_lamps)
{
    struct ge g;
    struct ge_console c;

    ge_init(&g);
    ge_log_set_active_types(0);
    ge_clear(&g);

    /* Raise both faults the way the machine raises them, then look. */
    g.mem_check = 1;
    g.inv_add   = 1;
    g.ALAM      = 1;    /* operator call, the third lamp CLEAR takes down */
    ge_fill_console_data(&g, &c);
    ASSERT_EQ((int)c.lamps.MEM_CHECK, 1);
    ASSERT_EQ((int)c.lamps.INV_ADD, 1);
    ASSERT_EQ((int)c.lamps.OPERATOR_CALL, 1);

    ge_clear(&g);

    ge_fill_console_data(&g, &c);
    ASSERT_EQ((int)c.lamps.MEM_CHECK, 0);
    ASSERT_EQ((int)c.lamps.INV_ADD, 0);
    ASSERT_EQ((int)c.lamps.OPERATOR_CALL, 0);
    ASSERT_EQ((int)c.lamps.HALT, 1);        /* and the machine is stopped */
}

/*
 * The machine has core where its straps say it has core.
 *
 * cp06 ch.080 MEMORY STARTING LOGIC: an address past the installed capacity
 * never starts a memory cycle. This machine's straps (E05 PONT2N + F05 PONT2P)
 * are the printed UCE 464 row: 32K, so 0x7FFF is there and 0x8000 -- which a
 * change register plus displacement can still address -- is not.
 */
UTEST(console_fidelity, memory_bound_follows_the_straps)
{
    struct ge g;

    ge_init(&g);
    ge_log_set_active_types(0);
    ge_clear(&g);

    ASSERT_EQ((int)ge_memory_capacity_k(&g), 32);
    ASSERT_TRUE(ge_mem_in_bounds(&g, 0x3FFF));
    ASSERT_TRUE(ge_mem_in_bounds(&g, 0x7FFF));   /* the 32K row has it all */

    /* MVI 0xAA, 0x4000: inside 32K, so it lands and no fault is raised. */
    g.mem[0] = 0x92;            /* MVI */
    g.mem[1] = 0xAA;            /* immediate */
    g.mem[2] = 0x40;            /* address hi */
    g.mem[3] = 0x00;            /* address lo */
    g.mem[4] = 0x0A;            /* HLT */
    ge_start(&g);
    for (int i = 0; i < 200 && !ge_halted(&g); i++)
        ge_run_cycle(&g);

    ASSERT_EQ((int)g.mem[0x4000], 0xAA);
    ASSERT_EQ((int)g.inv_add, 0);

    /* ... and 0x8000 is past the largest build ch.001 defines. An instruction
     * cannot name it directly -- bit 15 of an address field is the modifier
     * flag -- so reach it the way a program would: change register 1 = 0x8000
     * and a modified field 0x9000 (bit15 | reg 1 | disp 0). The write is
     * dropped and INV ADD comes up. */
    ge_init(&g);
    ge_clear(&g);
    g.mem[0x00F2] = 0x80;       /* change register 1 = 0x8000 */
    g.mem[0x00F3] = 0x00;
    g.mem[0] = 0x92;            /* MVI 0xAA, 0x000(1) */
    g.mem[1] = 0xAA;
    g.mem[2] = 0x90;
    g.mem[3] = 0x00;
    g.mem[4] = 0x0A;            /* HLT */
    ge_start(&g);
    for (int i = 0; i < 200 && !ge_halted(&g); i++)
        ge_run_cycle(&g);

    ASSERT_EQ((int)g.mem[0x8000], 0x00);
    ASSERT_EQ((int)g.inv_add, 1);
}

/*
 * The operator can change PO from the console and run from it.
 *
 * The procedure is the panel's own (CPU[4] §4.2's rotary table): CLEAR, turn
 * the register selector to PO, key the address into the AM toggles with INAR
 * inserted, START to perform the forcing cycle, back to NORM, START to run.
 *
 * Two things used to defeat it. A HLT parks the sequencer mid-phase with the
 * halted instruction still in FO, and CLEAR did not preset it, so the next
 * START finished the OLD instruction and consumed the forced address as its
 * operand. And the display sequence's CI15 row (NO <- L1) fired for every
 * rotary position, overwriting the PO that TO10 had just routed into the knot,
 * which state 80's `CO00 PO <- NI` then wrote back into the program addresser:
 * the machine ran from the last program's L1. See msl-states.c state_00.
 */
UTEST(console_fidelity, po_can_be_forced_and_run_from)
{
    struct ge g;
    struct ge_console_switches s = { 0 };

    ge_init(&g);
    ge_log_set_active_types(0);
    ge_clear(&g);

    /* MVI 0xAA, 0x0200 ; HLT  at 0x0000 */
    g.mem[0] = 0x92; g.mem[1] = 0xAA; g.mem[2] = 0x02; g.mem[3] = 0x00;
    g.mem[4] = 0x0A;

    ge_start(&g);
    for (int i = 0; i < 200 && !ge_halted(&g); i++)
        ge_run_cycle(&g);
    ASSERT_EQ((int)g.mem[0x0200], 0xAA);      /* it ran once */
    ASSERT_EQ((int)g.rPO, 0x0004);            /* and stopped on the HLT */

    g.mem[0x0200] = 0x00;                     /* wipe the evidence */

    /* CLEAR, then force PO back to 0x0000 from the panel. */
    ge_clear(&g);
    ge_set_console_rotary(&g, RS_PO);
    s.AM   = 0x0000;
    s.INAR = 1;
    ge_set_console_switches(&g, &s);
    ge_run_cycle(&g);
    ge_start(&g);                             /* START = one forcing cycle */
    ge_run_cycle(&g);
    ASSERT_EQ((int)g.rPO, 0x0000);            /* the address is keyed in */

    /* Back to NORM and run: the machine must start at what was forced. */
    s.INAR = 0;
    ge_set_console_rotary(&g, RS_NORM);
    ge_set_console_switches(&g, &s);
    ge_start(&g);
    for (int i = 0; i < 400 && !ge_halted(&g); i++)
        ge_run_cycle(&g);

    ASSERT_EQ((int)g.mem[0x0200], 0xAA);      /* it ran again, from 0x0000 */
    ASSERT_EQ((int)g.rPO, 0x0004);
}

/*
 * The storage key-in runs through core until the error stop ends it.
 *
 * Rotary position 8 (V1-SCR) stores the AM switches at V1 and advances V1.
 * fo.98's end-of-cycle stop exempts position 8 along with NORM, so with neither
 * PAPA nor a step switch inserted the machine does NOT stop after one store: it
 * goes on keying the same byte into address after address, through the whole of
 * core, until the addresser walks off the installed memory and INV ADD stops it
 * -- ALTO set, INV ADD lit. That is what the machine at Electric Dreams does,
 * and it is why INAR exists: it inhibits that stop (CPU[4] §4.2, fo.35).
 */
UTEST(console_fidelity, storage_key_in_runs_until_the_error_stop)
{
    struct ge g;
    struct ge_console_switches s = { 0 };

    ge_init(&g);
    ge_log_set_active_types(0);
    ge_clear(&g);

    /* V1 = 0x0002 -- a register forcing, which IS one cycle: the end-of-cycle
     * stop applies to every off-NORM position except the key-in. */
    ge_set_console_rotary(&g, RS_V1);
    s.AM = 0x0002;
    ge_set_console_switches(&g, &s);
    ge_run_cycle(&g);
    ASSERT_EQ(ge_console_start(&g), 0);
    ge_run_cycle(&g);
    ASSERT_TRUE(ge_halted(&g));
    ASSERT_EQ((int)g.rV1, 0x0002);

    /* V1-SCR = 0x03, and let the machine run as a front end would. */
    ge_set_console_rotary(&g, RS_V1_SCR);
    s.AM = 0x0003;
    ge_set_console_switches(&g, &s);
    ge_run_cycle(&g);
    ge_console_start(&g);

    int cycles = 0;
    while (!ge_halted(&g) && cycles < 200000) {
        ge_run_cycle(&g);
        cycles++;
    }

    ASSERT_TRUE(ge_halted(&g));                  /* it stopped by itself */
    ASSERT_EQ((int)g.inv_add, 1);                /* on INV ADD */
    ASSERT_EQ((int)g.mem[0x0002], 0x03);         /* having keyed from V1 up */
    ASSERT_EQ((int)g.mem[0x1000], 0x03);
    ASSERT_EQ((int)g.mem[0x7FFF], 0x03);         /* to the top of the 32K */
    ASSERT_GE((int)g.rV1, 0x8000);               /* and off the end */

    /* PAPA is what makes it one byte per press: with it inserted the machine
     * stops at the end of the microsequence like any other forcing. */
    ge_init(&g);
    ge_clear(&g);
    memset(&s, 0, sizeof(s));
    s.PAPA = 1;
    ge_set_console_rotary(&g, RS_V1);
    s.AM = 0x0002;
    ge_set_console_switches(&g, &s);
    ge_run_cycle(&g);
    ge_console_start(&g);
    ge_run_cycle(&g);

    ge_set_console_rotary(&g, RS_V1_SCR);
    s.AM = 0x0003;
    ge_set_console_switches(&g, &s);
    ge_run_cycle(&g);
    ge_console_start(&g);
    for (int i = 0; i < 64 && !ge_halted(&g); i++)
        ge_run_cycle(&g);

    ASSERT_TRUE(ge_halted(&g));
    ASSERT_EQ((int)g.mem[0x0002], 0x03);
    ASSERT_EQ((int)g.mem[0x0003], 0x00);         /* exactly one store */
    ASSERT_EQ((int)g.inv_add, 0);

    /* Read it back. Position 9 is a read-out: with the rotary there the RO
     * lamps show the byte AT V1 for as long as the panel runs -- the display
     * cycle fetches it, which is the only way anything stays on those lamps
     * (RO is cleared at TO20 of every cycle, fo.142) -- and each START steps
     * V1 on to the next byte. */
    ge_set_console_rotary(&g, RS_V1);
    s.AM = 0x0002;
    ge_set_console_switches(&g, &s);
    ge_run_cycle(&g);
    ge_console_start(&g);
    ge_run_cycle(&g);

    ge_set_console_rotary(&g, RS_V1_LETT);
    ge_set_console_switches(&g, &s);
    for (int i = 0; i < 4; i++)
        ge_run_cycle(&g);

    struct ge_console c = { 0 };
    ge_fill_console_data(&g, &c);
    ASSERT_EQ((int)c.lamps.RO, 0x03);            /* "3" on the lamps ... */
    ASSERT_EQ((int)g.rV1, 0x0002);               /* ... and V1 has not moved */

    for (int i = 0; i < 40; i++)                 /* it stays there */
        ge_run_cycle(&g);
    ge_fill_console_data(&g, &c);
    ASSERT_EQ((int)c.lamps.RO, 0x03);

    ge_console_start(&g);                        /* START steps to the next */
    for (int i = 0; i < 40 && !ge_halted(&g); i++)
        ge_run_cycle(&g);
    ASSERT_EQ((int)g.rV1, 0x0003);

    /* And at NORM the same key means run, and says so. */
    ge_set_console_rotary(&g, RS_NORM);
    ASSERT_EQ(ge_console_start(&g), 1);
}