#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "ge.h"
#include "signals.h"
#include "log.h"

/**
 * Compute the odd-parity bit for an 8-bit data value.
 *
 * Returns 0 if the data byte already has an odd number of 1-bits
 * (parity bit not needed), or 1 if an additional 1-bit is required
 * to make the total count of 1-bits (data + parity) odd.
 */
static inline uint8_t odd_parity(uint8_t data)
{
    /* __builtin_popcount gives the number of set bits; if it's already
     * odd we need parity=0; if even we need parity=1. */
    return (__builtin_popcount(data) & 1) ? 0 : 1;
}

/**
 * Does the machine have core at this address?
 *
 * The answer is the backplane's, not a setting: cp06 ch.080 MEMORY STARTING
 * LOGIC watches address bits 12-14 against the ch.001 capacity straps, and an
 * address past the installed capacity simply never starts a memory cycle
 * (ge_mem_in_bounds, signals.h). This machine is strapped 32K (E05 PONT2N +
 * F05 PONT2P, TAB.1's UCE 464 row), so all of 0x0000-0x7FFF is there.
 *
 * The 32K ceiling itself is not one of those gates: ch.080 only decodes bits
 * 12-14, because on the largest machine there is no bit 15 of memory to decode
 * -- 32K IS the top of the ch.001 table and of the physical build (two MEM470
 * boxes, 16K positions each). An address with bit 15 set can still be computed,
 * by a change register plus displacement, and it addresses core that no build
 * has; it gets the same INV ADD as any other absent address.
 *
 * `mem_size` stays as the harness override for tests that want a specific
 * capacity without restrapping the machine; 0 (the default) means "ask the
 * straps".
 */
#define GE_MAX_CORE 0x8000u   /* 32K: the largest build ch.001 defines */

static inline int mem_addr_installed(struct ge *ge, uint16_t addr)
{
    if (ge->mem_size)
        return addr < ge->mem_size;
    return addr < GE_MAX_CORE && ge_mem_in_bounds(ge, addr);
}

/**
 * The fault lamps, and the error stop.
 *
 * A memory fault does not just light a lamp: it stops the subsystem, which is
 * why CLEAR is "required after MEM CHECK" (CPU[4] §3.3) and why the maintenance
 * panel has a switch to turn the stop off -- INAR, "inhibits the error stop on
 * a memory check error or on addressing a non-existent address" (§4.2, fo.35).
 * The lamp is raised either way; only the stop is inhibited.
 *
 * This is what ends a console storage key-in. With the rotary at position 8 and
 * neither PAPA nor the step switch inserted, nothing stops the machine at the
 * end of a cycle (fo.98's ALSOA=0 exempts position 8 along with NORM), so it
 * goes on storing the AM switches at V1 and advancing V1 -- through the whole
 * of core, exactly as the machine at Electric Dreams does -- until the address
 * walks off the installed memory and INV ADD sets ALTO. Insert PAPA and each
 * START stores one byte instead.
 *
 * The LAMP is a condition, not a latch: it reports the memory cycle you are
 * looking at, and a good cycle puts it out again. That is only visible with
 * the stop inhibited -- INAR in, nothing loaded, START: the machine walks
 * zeroes up through core, and INV ADD comes on as the addresser passes the
 * installed memory and goes off again as it wraps to 0, blinking once per lap.
 * Observed on the restored machine, 2026-07-31. With INAR out the machine
 * stops ON the faulting cycle, so the lamp stands there lit, which is the
 * other half of the same behaviour -- and why CLEAR is "required after MEM
 * CHECK": what CLEAR releases is the stop.
 */
static inline void mem_fault(struct ge *ge, uint8_t *lamp)
{
    *lamp = 1;
    if (!ge->console_switches.INAR)
        ge->ALTO = 1;
}

static void on_TO00(struct ge *ge) {
    /* cpu fo. 115 */
    ge->RIA0 = ge->RC00 && !ge->ALTO;
    ge->RESI = ge->RC01;
    ge->RIA2 = ge->RC02;
    ge->RIA3 = ge->RC03;

    ge->RETO = RES01(ge);
    ge->RET2 = RES2(ge);   /* T010 latch, cp06 ch.132-6 (see ge.h RET2) */

    /* TODO: a "counter" with RAMO, RAMI should condition RIA0 */

    ge_log(LOG_CYCLE, "  async: RC00: %d RC01: %d RC02: %d RC03: %d ALTO: %d\n",
           ge->RC00, ge->RC01, ge->RC02, ge->RC03, ge->ALTO);

    ge_log(LOG_CYCLE, "  sync:  RIA0: %d RESI: %d RIA2: %d RIA3: %d\n",
           ge->RIA0, ge->RESI, ge->RIA2, ge->RIA3);

    ge_log(LOG_CYCLE, "      -> RIUC: %d RES0: %d RES2: %d RES3: %d\n",
           RIUC(ge), RES0(ge), RES2(ge), RES3(ge));

    /* set NI to output the counting network.
     * ("this occoursr alwas during the 1st phase", cpu fo.125) */

    ge->kNI.ni1 = NS_CN1;
    ge->kNI.ni2 = NS_CN2;
    ge->kNI.ni3 = NS_CN3;
    ge->kNI.ni4 = NS_CN4;

    /* CO41 used to set from_zero is issued in TO10, so this looks
     * like a reasonable place to reset this. CO40 (decreasing) must be reset
     * here too, otherwise a stale "decreasing" from one decrement turns every
     * later +1 advance into a -1 (now that the counting network honours it). */
    memset(&ge->counting_network.cmds, 0, sizeof(ge->counting_network.cmds));
    memset(&ge->counting_network.ci_cmds, 0, sizeof(ge->counting_network.ci_cmds));

    /* CI45/46/47 are state-local UA mode pulses. */
    memset(&ge->ua_controls, 0, sizeof(ge->ua_controls));
}

static void on_TO10(struct ge *ge) {
    ge->ffFA = ge->ffFI; /* cpu fo. 129  */
    ge->rSA  = NA_knot(ge); /* cpu fo. 128 */

    /* save SA to emulate the future state network */
    ge->future_state = ge->rSA;

    /* TODO: a "counter" with RAMO, RAMI should count (cpu fo. 115) */
}

static void on_TO11(struct ge *ge) {}
static void on_TO15(struct ge *ge) {}

static void on_TO19(struct ge *ge) {
    /* intermediate fo. 9 B1*/
    ge->RECE = 0;
}

static void on_TO20(struct ge *ge) {
    ge->rBO = NO_knot(ge); /* cpu fo. 142, 126 */
    ge->rVO = NO_knot(ge); /* cpu fo. 124, 125 */

    ge->kNO.forcings = 0;
    ge->kNO.force_mode = KNOT_FORCING_NONE;

    ge->ACIC = 0; /* cpu fo. 99  */
    ge->rRO = 0;  /* cpu fo. 142 */
}

static void on_TO25(struct ge *ge) {}
static void on_TO30(struct ge *ge) {}

static void on_TO40(struct ge *ge) {
    /* stub */
    if (ge->counting_network.cmds.from_zero) {
        ge->kNI.ni1 = NS_CN1;
        ge->kNI.ni2 = NS_CN2;
        ge->kNI.ni3 = NS_CN3;
        ge->kNI.ni4 = NS_CN4;
    }
}

static void on_TO50(struct ge *ge) {
    if (ge->PEC1_pending) {
        ge->PEC1 = 1;
        ge->PEC1_pending = 0;
    }

    /* not sure about the timing of memory ops
     * read was previously done in TO65 with write, but
     * it didn't work to implement the state CC for PERI.
     * reading here  seems to work in all known cases */
    if (ge->memory_command == MC_READ) {
        if (!mem_addr_installed(ge, ge->rVO)) {
            /* Address outside installed memory: raise INV ADD fault */
            mem_fault(ge, &ge->inv_add);
            ge_log(LOG_STATES, "memory read: INV ADD rVO=%x (bound %uK)\n",
                   ge->rVO, ge_memory_capacity_k(ge));
        } else {
            ge->inv_add = 0;              /* the address is there: condition gone */
            ge->rRO = ge->mem[ge->rVO];
            ge_log(LOG_STATES, "memory read: RO = mem[VO] = mem[%x] = %x\n",
                   ge->rVO, ge->rRO);

            /* Parity check. A location this machine has written carries a
             * check bit gemu generated, and comparing it is the whole point.
             *
             * A location it has NOT written is a question about the iron, not
             * about the model: core retains, so on a real machine every cell
             * holds something with the check bit whatever wrote it last left
             * behind, and reads clean. gemu's array starts virgin -- data 0
             * with check bit 0, which is an even count and therefore a parity
             * ERROR -- so checking it would report a fault the machine does
             * not have. Hence the exemption, and hence `mem_check_blank` for
             * the machine that really is in that state: powered up with core
             * never written, MEM CHECK stands on for as long as it runs
             * through it, which is what Electric Dreams shows with nothing
             * loaded. See ge.h. */
            if (ge->mem_written[ge->rVO] || ge->mem_check_blank) {
                if (odd_parity(ge->mem[ge->rVO]) != ge->mem_parity[ge->rVO]) {
                    mem_fault(ge, &ge->mem_check);
                    ge_log(LOG_STATES,
                           "memory read: MEM CHECK parity error at %x\n",
                           ge->rVO);
                } else {
                    ge->mem_check = 0;    /* checked, and good */
                }
            }
        }

        ge->memory_command = MC_NONE;
    }
}

static void on_TO50_1(struct ge *ge) {
    if (!ge->TO50_did_CI32_or_CI33) {
        /* timing chart js1-js2-jie-ecc, fo. 32,
         * also, display, fo. 17 */
        ge->rBO = NO_knot(ge);
    }

    ge->TO50_did_CI32_or_CI33 = 0;

    ge->kNO.forcings = 0;
    ge->kNO.force_mode = KNOT_FORCING_NONE;
}

static void on_TO60(struct ge *ge) {}
static void on_TO64(struct ge *ge) {}

static void on_TO65(struct ge *ge) {
    /* not sure about the timing of memory ops
     * in cpu fo. 145, "write" seems to be at around TO65,
     * and the "test k" fails if it's in TO50. */

    if (ge->memory_command == MC_WRITE) {
        if (!mem_addr_installed(ge, ge->rVO)) {
            /* Address outside installed memory: raise INV ADD fault, skip store */
            mem_fault(ge, &ge->inv_add);
            ge_log(LOG_STATES, "memory write: INV ADD rVO=%x (bound %uK)\n",
                   ge->rVO, ge_memory_capacity_k(ge));
        } else {
            ge->inv_add = 0;              /* the address is there: condition gone */

            uint8_t parity = odd_parity(ge->rRO);

            /* Console check-bit forcing: during a storage forcing from the
             * console (rotary pos 8, RS_V1_SCR) with INCE inserted, AM08 is
             * stored as the parity bit and the normal parity generation for
             * AM07-00 is inhibited. This lets the operator key in a wrong
             * check bit to exercise MEM CHECK detection. (CPU[4] §4.2, fo.36-37) */
            if (ge->register_selector == RS_V1_SCR && ge->console_switches.INCE)
                parity = (ge->console_switches.AM >> 8) & 1;

            ge->mem[ge->rVO] = ge->rRO;
            ge->mem_parity[ge->rVO]  = parity;
            ge->mem_written[ge->rVO] = 1;
            ge_log(LOG_STATES, "memory write: mem[VO] = RO = mem[%x] = %x (parity %d)\n",
                   ge->rVO, ge->rRO, parity);
        }

        ge->memory_command = MC_NONE;
    }

    /* "enables the second phase commands for count selection" (cpu fo.142):
     * the CO-phase counting flags are replaced by the CI-phase staging, so
     * the TI05 loads (CI05 NI->L1, ...) see the CI40/41/42/44 selection. */
    ge->counting_network.cmds = ge->counting_network.ci_cmds;
    memset(&ge->counting_network.ci_cmds, 0, sizeof(ge->counting_network.ci_cmds));
}

static void on_TO70(struct ge *ge) {
    if (ge->PEC1)
        ge->RASI = 0;
}

static void on_TO80(struct ge *ge) {}

static void on_TO89(struct ge *ge) {
    ge->PEC1 = 0;
    ge->PEC1_pending = 0;
}

static void on_TO90(struct ge *ge) {
    /* TODO: check if ! is correct: PODIB should be PODI negated */
    if (!ge->PODI)
        ge->ACIC = 1;  /* cpu fo. 99 */
}

static void on_TI05(struct ge *ge) {
    /* TODO: check if ! is correct: PODIB should be PODI negated */
    if (ge->PODI)
        ge->ACIC = 1;  /* cpu fo. 99 */
}

static void on_TI06(struct ge *ge) {}
static void on_TI10(struct ge *ge) {}

static on_pulse_cb pulse_cb[END_OF_STATUS] = {
    on_TO00,
    on_TO10,
    on_TO11,
    on_TO15,
    on_TO19,
    on_TO20,
    on_TO25,
    on_TO30,
    on_TO40,
    on_TO50,
    on_TO50_1,
    on_TO60,
    on_TO64,
    on_TO65,
    on_TO70,
    on_TO80,
    on_TO89,
    on_TO90,
    on_TI05,
    on_TI06,
    on_TI10,
};


void pulse(struct ge *ge)
{
    if (pulse_cb[ge->current_clock]) {
        pulse_cb[ge->current_clock](ge);
    }
}
