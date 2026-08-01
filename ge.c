#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include "ge.h"
#include "signals.h"
#include "msl.h"
#include "console_socket.h"
#include "peripherical.h"
#include "log.h"

#define MAX_PROGRAM_STORAGE_WORDS 129

void ge_init(struct ge *ge)
{
    memset(ge, 0, sizeof(*ge));
    /* Strapped as the machine at Electric Dreams, per the physical card
     * identification of 2026-07-21: 0618034Z reads PONT2N on the board, and
     * 0618035V is electrically a PONT2N as well (different part code, same
     * strap).  E03 = PONT2N is the UCE 468 row of TAB.1 (2 usec, MAX), so
     * the assumed model survives the re-identification; F03 = PONT2N flips
     * TAB.2 to "interruption enabled on connector 3" (it was modelled as
     * connector 4).  E04/F04 are believed empty, pending a physical check.
     *
     * (2026-07-21: the F03 card was found and restuffed; all four option
     * sockets hold the same 4-bridge card type, annotated PONT2N.
     * 2026-07-22: the cards' own catalog drawing was found -- cp10F dwg
     * 015 433 91 -- and settles the type: PONT 2N is a FOUR-bridge recipe
     * (holes 3/20/21/25) shorting pins {1,2,4,7} to 17, PONT 2P likewise
     * four bridges differing in one (pin 3 for pin 4).  The in-machine
     * cards match the 2N recipe, which retires the {1,3,4}-union reading;
     * the pure-N strapping below stands.  Final electrical seal: pin 1<->4
     * beeps (2N) and pin 1<->3 does not.  docs/hardware-options.md.)
     *
     * Card 05, the memory-capacity pair, is the machine's OWNER's reading
     * (2026-07-31): the boards say **32K**, which is TAB.1's printed
     * {E05, F05} = {PONT2N, PONT2P} row -- UCE 464, (VAMA2,VEMB6,VAMC2) =
     * (0,0,0) -- and it agrees with the physical build (2x MEM470 mounted,
     * Q30/Q31 read amplifiers populated: docs/hardware-options.md).  It
     * supersedes the 2026-07-21 photo reading of BOTH 05 cards as PONT2N,
     * which landed off the ch.001 table at (1,0,0) and bounded the machine
     * at 16K -- a bound that made every deck stop short of the memory it
     * has, and that no printed row defines.  If a card is ever re-buzzed and
     * F05 really is a 2N, flip it back here and the ch.080 gates will bound
     * the machine again without anything else changing. */
    ge->options.E03 = PONT_2N;   /* UCE 468: 2 usec, MAX                     */
    ge->options.F03 = PONT_2N;   /* interruption enabled on connector 3      */
    ge->options.F04 = PONT_NONE; /* empty = the interrupts-enabled variant   */
    ge->options.E04 = PONT_NONE; /* loading enabled on connectors 2 and 3    */
    ge->options.E05 = PONT_2N;   /* UCE 464: 32K core, TAB.1's {N,P} row     */
    ge->options.F05 = PONT_2P;   /* -> (VAMA2,VEMB6,VAMC2) = (0,0,0)         */

    ge->ALTO = 1;      /* stopped until CLEAR + START */
    ge->powered = 1;
    ge->register_selector = RS_NORM;

    ge->ST3.name = "ST3";
    ge->ST4.name = "ST4";

    ge->channel2.name = "CAN2";

    /* Power-on, and ONLY power-on, establishes the identity change registers.
     * They are core cells at 0x00F0-0x00FE; core retains, so CLEAR must not
     * touch them (see ge_clear). A machine that has been powered up all day
     * holds whatever the last program left there -- which is exactly how the
     * bench found a compiled program's frame pointer landing at 0x0600 instead
     * of the assumed 0x6000. */
    ge_seed_segment_bases(ge);
}

/*
 * Resolve every strapped option and report it once, at CLEAR.
 *
 * The FUL / FEL / VAM / INES signals are not free-standing settings: each is
 * a level produced by a jumper card in a backplane connector, so the only
 * thing that is really configured is which cards are fitted where.  This maps
 * that to the signals the logic actually reads, which is what you want when
 * comparing gemu against a physical machine -- the levels here should match
 * what a meter reads on the corresponding connector pins.
 *
 * cp06 ch.001 (memory capacity) and ch.002 (version, loading, interruptions);
 * see docs/hardware-options.md.
 */
static const char *pont_name(enum ge_pont p)
{
    switch (p) {
        case PONT_2N: return "PONT2N";
        case PONT_2P: return "PONT2P";
        default:      return "(empty)";
    }
}

void ge_log_options(struct ge *ge)
{
    {
        /* Capacity is the gate-derived ch.080 bound, which is defined for
         * every strap combination; flag the ones the ch.001 table does not
         * print, since they are configurations the factory never shipped. */
        uint8_t a = VAMA2(ge), b = VEMB6(ge), c = VAMC2(ge);
        uint8_t printed = (a && b) || (!a && !b) || (a && !b && c);
        ge_log(LOG_DEBUG, "options: UCE %u processor, %u ns cycle, %s set; "
                          "%uK core%s\n",
               ge_cpu_version_uce(ge), ge_cycle_period_ns(ge),
               ge_cpu_version_uce(ge) == 466 ? "MIN" : "MAX",
               ge_memory_capacity_k(ge),
               printed ? "" : " (off-table straps, bound per ch.080)");
    }

    ge_log(LOG_DEBUG, "options: straps E03=%s E04=%s E05=%s "
                      "F03=%s F04=%s F05=%s  S42=%s\n",
           pont_name(ge->options.E03), pont_name(ge->options.E04),
           pont_name(ge->options.E05), pont_name(ge->options.F03),
           pont_name(ge->options.F04), pont_name(ge->options.F05),
           ge->options.S42_diag ? "DIAG" : "normal");

    /* ch.002: loading, version and the FUL4 pair. */
    ge_log(LOG_DEBUG, "options: FUL26=%u FUL36=%u -> load on connectors %s; "
                      "FEL06=%u FEL16=%u FUL4G=%u FUL4F=%u\n",
           FUL26(ge), FUL36(ge),
           FUL26(ge) ? (FUL36(ge) ? "2 and 3" : "2 and 4") : "4 and 3",
           FEL06(ge), FEL16(ge), FUL4G(ge), FUL4F(ge));

    /* ch.002 TAB.2: interruption-enabled connectors. */
    ge_log(LOG_DEBUG, "options: INES3=%u INES4=%u -> interruptions %s\n",
           INES3(ge), INES4(ge),
           (INES3(ge) && INES4(ge)) ? "on connectors 3 and 4" :
           INES3(ge) ? "on connector 3" :
           INES4(ge) ? "on connector 4" : "disabled");

    /* ch.001: memory capacity selection. */
    ge_log(LOG_DEBUG, "options: VAMA2=%u VEMB6=%u VAMC2=%u "
                      "(VAMA1=%u VAMB1=%u VAMC1=%u)\n",
           VAMA2(ge), VEMB6(ge), VAMC2(ge),
           VAMA1(ge), VAMB1(ge), VAMC1(ge));
}

void ge_clear(struct ge *ge)
{
    ge->AINI = 0;
    ge->ALAM = 0;
    ge->PODI = 0;
    ge->ADIR = 0;
    ge->ACIC = 1;

    /* After the powering on of the machine the timing starts pressing the
     * "CLEAR" switch (cpu fo. 99).  The delay line runs; the CPU does not --
     * ALTO is set just below, and START (cpu fo. 97) is what releases it. */

    /* (One of) the possible set conditions (is): or with
     * "CLEAR" and.. (cpu fo. 98) */
    ge->ALTO = 1;

    /* By pressing "CLEAR" tje FF RC01, RC02, RC03 are reset and the FF
     * RC00 is set. (cpu fo. 115) */
    ge->RC00 = 1;
    ge->RC01 = 0;
    ge->RC02 = 0;
    ge->RC03 = 0;

    /* REGEN: general clear of the integrated-reader command/mode latches (the
     * controller's reset line). Pulse `regen` and drop the CPU->reader command
     * lines + the CPU-selected read mode so a CLEAR returns the reader to its
     * power-up state. (Inert: the data path and the harness-selected transcode
     * mode are unaffected; active_valid==0 keeps the harness mode in force.) */
    ge->integrated_reader.regen = 1;
    ge->integrated_reader.tu00 = 0;
    ge->integrated_reader.tu03 = 0;
    ge->integrated_reader.rifan = 0;
    ge->integrated_reader.sesen = 0;
    ge->integrated_reader.cocon = 0;
    ge->integrated_reader.mode_n001 = 0;
    ge->integrated_reader.mode_n002 = 0;
    ge->integrated_reader.mode_debi = 0;
    ge->integrated_reader.mode_mi01 = 0;
    ge->integrated_reader.mode_mi02 = 0;
    ge->integrated_reader.active_valid = 0;
    ge->integrated_reader.luren = 0;   /* transcoder/jam error: an error condition */
    ge->PEC1_pending = 0;

    /* "Clears all error conditions" (CPU[4] §3.3). These are the two the
     * operator panel shows, and the reason the manual says CLEAR is *required*
     * after MEM CHECK: the fault latches are what stops the subsystem, and
     * nothing else in the machine takes them down. MEM CHECK is the parity
     * fault (pulse.c on_TO50), INV ADD the address-past-installed-core fault
     * (both memory phases) -- neither is a momentary condition, so leaving them
     * standing across a CLEAR left the panel lit for a fault the operator had
     * already acknowledged. */
    ge->mem_check = 0;
    ge->inv_add   = 0;

    /* The condition flip-flops are part of the preset state: FI carries the
     * 2-bit condition code the jumps test (alu_cc.c) and FA its console-visible
     * copy, and a machine just cleared must not answer a JC with the last
     * program's result. */
    ge->ffFI = 0;
    ge->ffFA = 0;
    ge->JE   = 0;

    /* The channel latches go with them. RIG1 is "end from controller 1", set
     * when a transfer's last character arrives (reader.c, with FININ) and
     * normally taken down by CE03 inside the next order -- which the load
     * sequence never issues. Left standing it says the transfer that has not
     * started yet has already finished, so the IPL reads nothing: a second deck
     * could not be loaded without powering the machine off, which is not a
     * machine anyone could work with. The operator loads deck after deck, and
     * CLEAR between them is what makes that possible. RACI (rejected command)
     * and RECE (selection check) are error conditions in the same breath. */
    ge->RIG1 = 0;
    ge->RIG3 = 0;
    ge->RACI = 0;
    ge->RAVI = 0;
    ge->RECE = 0;
    ge->PEC1 = 0;

    /* And the working registers, which is the rest of what "presets the CPU to
     * a defined state" has to mean. They are not core: V1-V4 stage operand
     * addresses within a cycle, L1-L3 lengths, FO the opcode being executed, RO
     * the memory data register (cleared at TO20 of every cycle anyway). The
     * program addresser PO is deliberately NOT among them -- the first START
     * after CLEAR runs the program from where it is parked.
     *
     * This is what a second LOAD needs. The bootstrap builds its order out of
     * the knot, and the knot is fed by these: with the last run's values still
     * in them the load read an order of 0x41 -- one bit off "read unchanged",
     * an anomaly at the reader -- and the machine sat in the input wait having
     * asked for nothing. A machine you cannot load twice without switching it
     * off is not the machine. */
    ge->rV1 = ge->rV2 = ge->rV3 = ge->rV4 = 0;
    ge->rL1 = ge->rL3 = 0;
    ge->rL2 = 0;
    ge->rFO = 0;
    ge->rRO = 0;
    ge->rBO = 0;

    /* The program addresser goes to zero with them, and this is the row that
     * makes CLEAR -> LOAD -> START work twice.
     *
     * LOAD "sets AINI and nothing else" (CPU[4] §3.3), so it is CLEAR that has
     * to leave the machine somewhere the bootstrap can start: the IPL reads one
     * card to 0x0000 and executes it there, and gemu builds the load's
     * addresses out of the knot, which the display sequence feeds with PO. With
     * the last program's PO still standing -- 0x010c, where print.cap halts --
     * the load went looking for its order block up there and asked the reader
     * for 0x41 instead of 0x40: one bit off "read unchanged", an anomaly at the
     * reader, and the machine sat in the input wait having asked for nothing.
     * A deck could be loaded once per power-on.
     *
     * Resuming a halted program is unaffected: that is START on its own, which
     * is what an operator presses. CLEAR is how you say start over -- and the
     * engineer keying an address into PO does it after the CLEAR, not before. */
    ge->rPO = 0;

    /* And the defined state the sequencer is preset TO is the display state.
     *
     * This is the one that bites the operator. A HLT parks the machine
     * mid-phase -- SO = e0 with the halted instruction still in FO -- and
     * without this the stale phase survives the CLEAR: the next START finishes
     * the OLD instruction, consuming whatever the operator has just forced into
     * PO as its operand address, and the program runs from two bytes past
     * wherever it was told to start. Changing PO from the console then does
     * nothing, which is not what the panel is for (CPU[4] §4.2's rotary table
     * exists to key PO and run from it).
     *
     * 00 is where a stopped GE-120 sits: the display sequence, which is what
     * puts the registers on the panel lamps. Its chart ends in CU07 -> 0x80,
     * Initialisation, and 0x80 goes to c8 with AINI set (LOAD pressed) or to
     * the alpha phase without it -- so one START after a CLEAR either runs the
     * load or fetches the next instruction AT PO, which is exactly what §3.3
     * says the first START after CLEAR does. */
    ge->rSO = 0x00;
    ge->rSA = 0x00;
    ge->future_state = 0x00;

    /* CLEAR does NOT clear core. It "presets CPU + peripherals to a defined
     * state" (CPU[4] §3.3) -- flip-flops, not memory. The change registers live
     * in core at 0x00F0-0x00FE and survive, along with every other byte the
     * last program wrote. Seeding them here would invent a reset identity the
     * machine does not have; ge_init does it once, at power-on. */

    ge_log_options(ge);
}

/* Seed the eight change / segment-base registers to their identity defaults
 * N<<12: change register N is the 16-bit big-endian word at mem[240+2N], and
 * an instruction address with modifier N (address bits 12-14) resolves to
 * displacement + base[N]. With these defaults a bare 12-bit displacement
 * carrying modifier N addresses segment N (0x1000*N ..), so a program's paged
 * addresses (e.g. JU 0x172a) resolve to their full load addresses; programs
 * may reload a base via LR/LA for paged access.
 *
 * Called by ge_clear (reset) and re-applied after a direct binary image load,
 * because a contiguous image spanning the 0x00F0-0x00FF window would otherwise
 * overwrite the bases (with its own bytes, or zeros in reconstructed gaps). */
void ge_seed_segment_bases(struct ge *ge)
{
    for (int n = 0; n < 8; n++) {
        uint16_t v = (uint16_t)(n << 12);
        ge->mem[240 + 2 * n]     = (uint8_t)(v >> 8);
        ge->mem[240 + 2 * n + 1] = (uint8_t)(v & 0xff);
        ge->cr_cache[n]          = v;   /* seed the addressing cache to match */
    }
}

/* odd-parity bit for a byte: 1 if the byte has an even number of set bits
 * (so data+parity is odd). Mirrors odd_parity() in pulse.c. */
static inline uint8_t ge_odd_parity(uint8_t data)
{
    return __builtin_parity(data) ? 0 : 1;
}

/* Store a byte to memory the way every real write does: data + generated odd
 * parity + mark written. The microcoded path (pulse.c on_TO65) does this for
 * itself (and honours the INCE check-bit forcing); this helper is for the
 * hybrid ALU/SS execution helpers (alu_*.c) and the change-register store,
 * which write ge->mem[] directly and would otherwise leave stale parity and
 * trip a false MEM CHECK on read-back. */
void ge_mem_store8(struct ge *ge, uint16_t addr, uint8_t val)
{
    ge->mem[addr]         = val;
    ge->mem_parity[addr]  = ge_odd_parity(val);
    ge->mem_written[addr] = 1;
}

/* Write a flat image into core at `origin`, priming the parity store and
 * marking the cells written, so reads of it parity-check cleanly.
 *
 * NOT A LOAD PATH. The machine takes programs from cards and from nothing else.
 * This is the bench engineer's hand: test scaffolding, and the model behind the
 * maintenance panel's memory key-in. Returns 0, or -1 past installed memory. */
int ge_load_image(struct ge *ge, const uint8_t *image, size_t size,
                  uint16_t origin)
{
    uint32_t max = ge->mem_size ? ge->mem_size : MEM_SIZE;

    if (image == NULL && size != 0)
        return -1;
    if ((uint32_t)origin + (uint32_t)size > max)
        return -1;

    for (size_t i = 0; i < size; i++) {
        uint16_t a = (uint16_t)(origin + i);
        ge->mem[a]         = image[i];
        ge->mem_parity[a]  = ge_odd_parity(image[i]);
        ge->mem_written[a] = 1;
    }
    return 0;
}

/* Force the sequencer into the alpha (fetch) phase at `entry`.
 *
 * NOT A MACHINE OPERATION -- there is no console control that does this. The
 * real entry is the IPL: CLEAR, LOAD1/2, LOAD, START walks 00 -> 80 -> c8 ...
 * -> e3 and begins executing the one card it read, at address 0. This helper
 * exists so unit tests can start a fragment mid-machine without a deck. */
void ge_enter(struct ge *ge, uint16_t entry)
{
    ge->rPO = entry;
    ge->rSO = 0xe2;   /* alpha phase: fetch the instruction at PO */
    ge->rSA = 0xe2;
}

void ge_load(struct ge *ge)
{
    /* When pressing LOAD button, AINI is set. If AINI is set, the state 80
     * (initialitiation) goes to state c8, starting the loading of the program
     * (of max 129 words) from one of the peripherc unit. */

    /* set AINI FF to 1 (pag. 96) */
    ge->AINI = 1;
}

void ge_load_1(struct ge * ge)
{
    /* It is possible to choose one between the two units thus prepared
     * positioning the operating console switch LOAD1/LOAD2 (The possible
     * choices are: Conn.2/Conn.3; Conn.4/Conn.3; Conn.2/Conn.4).
     *
     * (cpu fo. 43) */

    /* from the previous manual excerpt, ,i would have expected ALOI = 0t to
     * be LOAD1 and ALOI = 1 to be LOAD2, but running the initial load tests,
     * ALOI = 1 will result in the machine using the 0x80 unit name, which is
     * connector 2, while ALOI = 0 results in a 0x00 unit name, which is
     * connector 3. */

    ge->ALOI = 1;
}

void ge_load_2(struct ge * ge)
{
    ge->ALOI = 0;
}

void ge_start(struct ge *ge)
{
    /* according to the cpu documents, we should set the flipflop ARES here to
     * implement the initial loading of 80 into SO, however with the current
     * implementation it's not needed */

    ge->ALTO = 0; /* cpu fo. 97 */
}

static void ge_print_well_known_states(uint8_t state) {
    const char *name;
    switch (state) {
        case 0x00:
            name = "- Display sequence";
            break;
        case 0x08:
            name = "- Forcing sequence";
            break;
        case 0x64:
        case 0x65:
            name = "- Beta Phase";
            break;
        case 0x80:
            name = "- Initialitiation";
            break;
        case 0xE2:
        case 0xE3:
            name = "- Alpha Phase";
            break;
        case 0xF0:
            name = "- Interruption";
            break;
        default:
            name = "";
    }

    ge_log(LOG_STATES, "Running state %02x %s\n", state, name);
}

const char *ge_clock_name(enum clock c)
{
    switch (c) {
        #define X(name) \
            case name : \
                return #name ;
        ENUMERATE_CLOCKS
        #undef X
    }

    return "";
}

void ge_print_registers_nonverbose(struct ge *ge)
{
    if (ge_log_enabled(LOG_REGS_V))
        return;
    ge_log(LOG_REGS,
           "SO: %02x SA: %02x PO: %04x RO: %04x BO: %04x FO: %04x -  "
           "V1: %04x  V2: %04x V3: %04x  V4: %04x - "
           "L1: %04x  L2: %04x L3 : %04x\n",
           ge->rSO, ge->rSA, ge->rPO, ge->rRO, ge->rBO, ge->rFO,
           ge->rV1, ge->rV2, ge->rV3, ge->rV4,
           ge->rL1, ge->rL2, ge->rL3);
}

void ge_print_registers_verbose(struct ge *ge)
{
    ge_log(LOG_REGS_V,
           "%s:  "
           "SO: %02x SA: %02x PO: %04x RO: %04x BO: %04x FO: %04x  -  "
           "NO: %02x NI: %02x  -  "
           "FA: %02x FI: %02x - "
           "V1: %04x  V2: %04x V3: %04x  V4: %04x - "
           "L1: %04x  L2: %04x L3 : %04x\n",
           ge_clock_name(ge->current_clock),
           ge->rSO, ge->rSA, ge->rPO, ge->rRO, ge->rBO, ge->rFO,
           NO_knot(ge), NI_knot(ge),
           ge->ffFA, ge->ffFI,
           ge->rV1, ge->rV2, ge->rV3, ge->rV4,
           ge->rL1, ge->rL2, ge->rL3);
}

void ge_clock_increment(struct ge* ge)
{
    ge->current_clock++;
    if (ge->current_clock == END_OF_STATUS)
        ge->current_clock = TO00;
}

uint8_t ge_clock_is_first(struct ge* ge)
{
    return ge->current_clock == TO00;
}

uint8_t ge_clock_is_last(struct ge* ge)
{
    return ge->current_clock == (END_OF_STATUS - 1);
}

int ge_run_pulse(struct ge *ge)
{
    int r;
    const struct msl_timing_state *state;

    if (ge_clock_is_first(ge)) {
        r = ge_peri_on_clock(ge);
        if (r != 0)
            return r;

        /* poll the connectors and try to set up the cpu state.
         * should this be here? */
        connectors_first_clock(ge);
    }

    /* Execute common pulse machine logic */
    pulse(ge);

    /* Execute peripherals pulse callbacks */
    r = ge_peri_on_pulses(ge);
    if (r != 0)
        return r;

    /* Execute the commands from the timing charts */
    state =  msl_get_state(ge->rSA);

    /* The state to execute gets loaded in SA at TO10 */
    if (ge->current_clock == TO10)
        ge_print_well_known_states(ge->rSA);

    if (!state) {
        ge_log(LOG_ERR, "no timing charts found for state %02X\n", ge->rSA);
        return 1;
    }

    /* Latch the instruction-start PC for the disassembly display. In the alpha
     * fetch (e2/e3) PO addresses the opcode and is NOT advanced within the state
     * (operand fetch / PO recomputation happens in the later e0/e4/e6 states), so
     * this is the address of the instruction now executing. It stays put while
     * operands are read and PO is recomputed (e.g. across a jump), so a UI
     * highlight tracking it does not drift onto operand (DB) bytes or the next
     * line — it only moves when the next instruction is actually fetched. */
    if (ge->rSA == 0xe2 || ge->rSA == 0xe3)
        ge->instr_pc = ge->rPO;

    msl_run_state(ge, state);

    if (ge_clock_is_last(ge)) {
        fsn_last_clock(ge);
        ge_print_registers_nonverbose(ge);
    }

    ge_clock_increment(ge);
    return 0;
}

int ge_run_cycle(struct ge *ge)
{
    do {
        int r = ge_run_pulse(ge);
        if (r)
            return r;
    } while (!ge_clock_is_first(ge));

    return 0;
}

int ge_deinit(struct ge *ge)
{
    ge_peri_deinit(ge);
    return 0;
}

void connectors_first_clock(struct ge *ge)
{
    if (RA101(ge)) {
        ge_log(LOG_READER, "RA101: signaling incoming data\n");
        ge->RC01 = 1;
    }
}

void fsn_last_clock(struct ge *ge)
{
    /* At the end of a CPU cycle the future-status network is stored in SO
     * (cpu fo. 127), advancing the program sequencer one state.
     *
     * In maintenance forcing (rotary off NORM) the program sequencer is frozen:
     * the manual (CPU[4] §4 "Maintenance Panel", dwg 30004122 fo. 35-37) says a
     * forcing cycle writes the *register under exam* (displayed through BO), it
     * does not step the program. So a forcing cycle must NOT advance SO — that
     * is what lets the operator key an instruction across phases (force SO=E2,
     * step to E0, force FO, step to the 0x64 beta, force L1, step to execute).
     * The one exception is rotary position 13 (RS_SO), which forces SO/SI
     * itself — that is how the operator sets the sequencer state.
     *
     * RICI ("disable next status") suppresses the advance in normal operation,
     * letting a status be re-executed. */
    uint8_t sel_norm = ge->register_selector == RS_NORM;
    uint8_t sel_so   = ge->register_selector == RS_SO;
    uint8_t advance_so = sel_norm ? !ge->console_switches.RICI : sel_so;
    if (ge->RIA0 && advance_so) {
        ge_log(LOG_FUTURE, "last clock cpu, %02x in SO\n", ge->future_state);
        ge->rSO = ge->future_state;
    } else {
        ge_log(LOG_FUTURE, "last clock cpu, SO held at %02x (RIA0 %d advance %d)\n",
               ge->rSO, ge->RIA0, advance_so);
    }

    /* after the end of a cpu work cycle, (ALTO / ALS71=1) is set if
     * the PAPA switch is inserted, or if the rotary switch is neither
     * in the normal position, nor in position 8 for recording in
     * memory ALSOA=0) (cpu fo. 98)
     */
    uint8_t is_papa = ge->console_switches.PAPA;
    uint8_t is_norm = ge->register_selector == RS_NORM;
    uint8_t is_scr  = ge->register_selector == RS_V1_SCR;

    /* PAPA steps the MICROSEQUENCES and is NOT gated by the program. The
     * INS/ENS inhibit and the STOC override belong to the operator panel's
     * STEP-BY-STEP switch (ASIN), which is a separate circuit stopping at each
     * instruction through CI891 -- see ge.h ASIN and msl-states.c
     * state_E2_E3_TO80_CI89. The two used to be one thing here, which made PAPA
     * silently ignorable by any program that had issued INS. */
    ge_log(LOG_FUTURE, "    papa: %d, norm: %d, scr: %d ==> %d\n",
           is_papa, is_norm, is_scr,
           ge->RIA0 && (is_papa || !(is_norm || is_scr)));

    if (ge->RIA0 && (is_papa || !(is_norm || is_scr)))
        ge->ALTO = 1;

    /* PATE stops the timing after every cycle of the delay line — a finer step
     * than PAPA and, unlike PAPA, it is not gated by the CPU/channel cycle
     * (RIA0/RIA2), so it does interfere with peripheral transfers. One START
     * then runs exactly one delay-line cycle. (CPU[4] §4, fo.35) */
    if (ge->console_switches.PATE)
        ge->ALTO = 1;

    /* after the execution of a channel 2 cycle, load the first
     * 4 bits of the future status network in SI. (cpu fo. 127) */
    if (ge->RIA2) {
        ge_log(LOG_FUTURE, "last clock ch2, %02x in SI\n", ge->future_state);
        ge->rSI = ge->future_state;
    }

}
