#ifndef GE_H
#define GE_H

#include <stdint.h>
#include <stddef.h>
#include "opcodes.h"
#include "console.h"
#include "reader.h"
#include "channel.h"

#define CLOCK_PERIOD 14000 /* in usec, interval between pulse lines */
#define MEM_SIZE 65536

#define ENUMERATE_CLOCKS \
    X(TO00) \
    X(TO10) \
    X(TO11) \
    X(TO15) \
    X(TO19) \
    X(TO20) \
    X(TO25) \
    X(TO30) \
    X(TO40) \
    X(TO50) \
    X(TO50_1) \
    X(TO60) \
    X(TO64) \
    X(TO65) \
    X(TO70) \
    X(TO80) \
    X(TO89) \
    X(TO90) \
    X(TI05) \
    X(TI06) \
    X(TI10) \
    X(END_OF_STATUS)

enum clock {
    #define X(name) name ,
    ENUMERATE_CLOCKS
    #undef X
};

struct ge_counting_network {
    struct cmds {
        uint8_t from_zero:1;
        uint8_t decresing:1;
        uint8_t from_04:1;   /* count injected at bit 04 (CI42) */
        uint8_t stop_07:1;   /* carry/borrow blocked past bit 07 (CI44) */
    } cmds;

    /* Second-phase (CI-side) counting selection: CI40/41/42/44 stage their
     * configuration here; it becomes active when TO65 "enables the second
     * phase commands for count selection" (cpu fo. 142) — i.e. the CO-phase
     * flags are replaced by these for the TI05 loads (CI05 NI->L1 etc.).
     * This is what makes the b9/b1 per-character L1 decrement effective. */
    struct cmds ci_cmds;
};

/* Arithmetic-unit mode lines selected by CI45/CI46/CI47 for the duration of
 * one sequencer state.  Their combinations are decoded by CI68/CI69 when the
 * UA result is admitted to the NI knot. */
struct ge_ua_controls {
    uint8_t logic:1;          /* CI45: logical-operation group */
    uint8_t decimal_and:1;    /* CI46: decimal / AND group */
    uint8_t subtract_xor:1;   /* CI47: subtract / XOR group */

    /* CI50, "OPERA SOLO UA1 / WORK ONLY UA1" (cp07 fo.142).  Not a function
     * select: cp06 ch.094 shows CI50B = /CI501 feeding the UZE71 and UZE81
     * NANDs, so asserting CI50 INHIBITS the inter-zone enables and leaves
     * only the low unit participating.  It gates the arithmetic unit's
     * WIDTH, which is what a digit-at-a-time decimal operation needs. */
    uint8_t low_zone_only:1;  /* CI50: inhibit UZE71/UZE81 */
};

struct ge_knot_no {
    uint8_t forcings;

    enum {
        KNOT_FORCING_NONE,
        KNOT_FORCING_NO_21,
        KNOT_FORCING_NO_43,
    } force_mode;

    enum {
        KNOT_PO_IN_NO,
        KNOT_V1_IN_NO,
        KNOT_V2_IN_NO,
        KNOT_V3_IN_NO,
        KNOT_V4_IN_NO,
        KNOT_L1_IN_NO,
        KNOT_L2_IN_NO,
        KNOT_L3_IN_NO,
        KNOT_AM_IN_NO,
        KNOT_RI_IN_NO_43,
        /* Explicit "no selection drives the knot": hardware selections are
         * pulses, so a knot with no active selection reads 0 at latch time.
         * gemu's cmd persists across pulses; NO_UNDRIVEN models the decay
         * where a sheet's data path depends on it (interruption D3). */
        KNOT_NONE_IN_NO,
    } cmd;
};

enum knot_ni_source {
    NS_ZERO,   /* quartet driven by nothing: non-storing knot reads 0 */
    NS_CN1,
    NS_CN2,
    NS_CN3,
    NS_CN4,
    NS_RO1,
    NS_RO2,
    NS_UA2,
    NS_UA1,
};

struct ge_knot_ni {
    enum knot_ni_source ni1;
    enum knot_ni_source ni2;
    enum knot_ni_source ni3;
    enum knot_ni_source ni4;
};

/**
 * Backplane option connectors and the maintenance LAMPS switch.
 *
 * cp06 chapter 002 "VARIANTI E OPZIONI / CHANGE AND OPTION" (dwg 140 130 65 6).
 * The machine is configured at FOUR backplane connector positions -- E03,
 * E04, F03, F04 -- and by one switch on the maintenance panel.  The position
 * names decode in the cp08 card-layout scheme as <ROW><CARD>, so E04 is row E
 * card 04, and cp08 pp11-12 independently confirm which signals land on each.
 * See docs/hardware-options.md for the tables and for what to probe on a
 * physical machine.
 */
/* Strap-card types actually printed in the ch.001/ch.002 tables: only PONT2N
 * and PONT2P exist.  "PONT2H" was a misread of PONT2N in the 1968 typeface,
 * caught when the physical machine's cards were identified (2026-07-21):
 * 0618034Z reads PONT2N on the board, and 0618035V is electrically a PONT2N
 * too, under a different part code. */
enum ge_pont { PONT_NONE = 0, PONT_2N, PONT_2P };

struct ge_options {
    /** E03: machine version, paired with F04 (TAB.1) -- FEL06 / FEL16. */
    enum ge_pont E03;

    /** E04: which two connectors are enabled for the initial LOAD (TAB.3). */
    enum ge_pont E04;

    /** F03: which connectors may raise an interruption (TAB.2). */
    enum ge_pont F03;

    /** F04: machine version straps -- cycle period and performance (TAB.1). */
    enum ge_pont F04;

    /** E05 / F05: memory capacity (cp06 ch.001, "SELEZIONE CAPACITA
     *  MEMORIA") -- 8K through 32K.  See ge_memory_capacity_k(). */
    enum ge_pont E05;
    enum ge_pont F05;

    /**
     * S42 "LAMPS" on the maintenance panel, in position DIAG.
     *
     * Per the note on ch.002, FUL01/FUL11/FUL4F normally follow
     * FEL06/FEL16/FUL4G; with S42 in DIAG they become 0 / 0 / 1 instead.
     */
    uint8_t S42_diag;
};

/**
 * The entire state of the emulated system, including registers, memory,
 * peripherals and timings.
 */
struct ge {
    /* Main clock */
    enum clock current_clock;
    uint8_t powered;

    /** Backplane straps and maintenance-panel options (cp06 ch.002). */
    struct ge_options options;

    /* Lists of events and operations for all
     * pulses
     */
    struct pulse_event *on_pulse[END_OF_STATUS];

    /**
     * Program addresser.
     *
     * The register used to scan the positions of the memory in which
     * the program instructions are recorded. (p.118).
     */
    uint16_t rPO;

    /**
     * Instruction-start PC (display aid, not a real register). Latched in the
     * alpha fetch (state e2/e3) to the address of the opcode being executed, so
     * UI disassembly can highlight the current instruction without drifting onto
     * operands or the next line as rPO advances mid-instruction.
     */
    uint16_t instr_pc;

    uint16_t rV1; ///< Addresser for the first operand
    uint16_t rV2; ///< Addresser for the second operand
    uint16_t rV3; ///< Addresser for external instructions using channel 3
    uint16_t rV4; ///< Addresser for external instructions using channel 2

    /**
     * Change/segment-register cache (kept in sync by the register
     * instructions via cr_wr16 and by ge_seed_segment_bases).
     *
     * NOTE: modified-address resolution no longer reads this cache. The
     * indexing micro-cycle ED|EC -> EF|EE (timing tables CPU[7] p64) reads
     * the change register from memory at 240+2N byte by byte, exactly as the
     * hardware does — so a memory write to 0xF0-0xFF DOES affect addressing,
     * as on the real machine. The cache remains only as a debugging/UI aid
     * pending a decision on removing it.
     */
    uint16_t cr_cache[8];

    /**
     * Photoprint register
     * 8-bit register used to store the photodisc codes.
     */
    uint8_t rRI;

    /**
     * Length of the operand
     *
     * 16-bit used to store the length of the operands or for information in
     * transit.
     */
    uint16_t rL1;
    uint8_t  rL2; ///< Auxiliary register
    uint16_t rL3; ///< Length of operands involving channel 3

    struct ge_knot_no kNO;

    /**
     * Knot driven by counting network, or by the UA to store the result of the
     * operation. UA may store MSB or LSB depending on the operation.
     */
    struct ge_knot_ni kNI;

    /**
     * Multipurpose 8+1 bit register
     *
     * Stores the read signal from memory (e.g. the result of transfer command MEM).
     */
    uint16_t rRO;

    /**
     * UA (arithmetic unit) output latch
     *
     * The UA continuously combines a byte of BO with RO; gemu latches its
     * output when a "UA in NI" command (CI68 high byte / CI69 low byte) is
     * issued, and the NI knot sources NS_UA2/NS_UA1 read it back. The binary
     * carry between the two byte passes travels through the URPE flip-flop
     * (reset by CO49). Used by the modified-address indexing micro-cycle
     * ED|EC -> EF|EE (timing tables CPU[7] p64).
     */
    uint8_t rUA;

    /**
     * Default memory addresser
     *
     * 16-bit register which is loaded in TO20 from NO, used to address memory for
     * read and write operations.
     */
    uint16_t rVO;

    /**
     * Default operator
     *
     * 16-bit register automatically loaded from NO, used to drive the UA (aka ALU)
     * and used to visualize the content of other registers on the operating panel of
     * the console
     */
    uint16_t rBO;

    /**
     * Current function code
     *
     * 8-bit register storing the function code of the instruction being executed.
     */
    uint8_t rFO;

    /**
     * Main sequencer
     *
     * Drives the NA knot when the cycle has been attributed to the CPU or
     * channel 1.
     *
     * It is used to establish the sequence for:
     *  - alpha phase for all internal and external instructions
     *  - beta phase of internal instructions
     *  - organisation phase (general beta) of external instructions
     *  - program loading
     *
     * Loaded from the future status network when signal SOC01 is activated,
     * provided the RICI key is not active, in the following cases:
     *  - the FF ARES has been set thru "CLEAR". This causes the machine to
     *    execute the status 00, and setting of SO07 using CU07, this will
     *    set the configuration of SO to 80.
     *  - the rotary switch is in forcing of SO. When a cycle is attributed
     *    to the CPU pressing "START", the AM00-07 keys are forced in SO.
     *  - at the end of a cycle attributed to the CPU, when the rotary switch
     *    is in normal position, the future status network is stored in SO.
     *  (cpu fo. 127)
     */
    uint8_t rSO;

    /**
     * Peripheral unit sequencer
     *
     * 4-bit sequencer used for data xechange with peripheral units through
     * channel 2.
     *
     * Drives the NA knot when the cycle has been attributed to channel 2.
     *
     * Loaded with the first 4 bits of the future status network
     *  - after the execution of a channel 2 cycle
     *  - when forcing a status in SI using CU20 (DC status of general beta phase)
     */
    uint8_t rSI;

    /**
     * Future state configuration
     *
     * Register that drives the MLS and the logic to generate future status
     * configuration
     */
    uint8_t rSA;

    uint8_t rRE;
    uint8_t rRA;

    /**
     * Special conditions register 1
     *
     * 7 Flip-Flops containing special conditions which occur during the performance
     * of an instruction. Unloaded in #ffFA in T010
     */
    uint8_t ffFI;

    /**
     * Special conditions register 2
     *
     * 7 Flip-Flops containing special conditions which occur during the performance
     * of an instruction. Loaded from #ffFI in T010.
     *
     * Faults (from pp. 139-141)
     */
    uint8_t ffFA;

    /* Those two store which work channel the present cycle as been attributed to
     * (cpu fo. 130).
     * When setting RETO, if PAPA switch is set, rotary is neither in normal, nor
     * in position 8 to store in memory, should set ALTO */
    uint8_t RETO:1;

    /**
     * Channel-2 cycle-assignment memory.
     *
     * T010-clocked latch of RES2 (cp06 ch.132-6/7/8, "OPERATING SIMULTANEITY
     * LOGIC": RET21 = /((RES2A·T0107)+(T010C·RET2A)), RET2A = /RET21,
     * RET26 = /RET2A). Remembers that the current cycle was assigned to
     * channel 2 — including the priority masking in RES2 (!RIA3 && !RESI
     * && RIA2), which a raw RIA2 read would miss. Twin of RETO.
     */
    uint8_t RET2:1;

    /**
     * Program Loading
     *
     * Set by pressing the "LOAD" button of the console, and it is reset by pressing
     * "CLEAR", or with the command CI39 (in the alpha phase of the E0 state).
     */
    uint8_t AINI:1;

    /** 
     * Load connector selection
     *
     * Set by toggling the bistable switch "LOAD 1"/"LOAD 2" button of the console.
     */
    uint8_t ALOI:1;

    /**
     * Stops internal cycles
     *
     * If set, stops the performance of the internal processing cycles, without
     * stopping the timing generation (cpu fo. 98).
     */
    uint8_t ALTO:1;

    /**
     * Slow delay line
     *
     * Increases the delay line cycle by about 130ns. It is set together with
     * ALAM by the LOLL diagnostic instruction (cpu fo. 96).
     */
    uint8_t PODI:1;

    /**
     * Recycle delay line
     *
     * Initially is set by "CLEAR", after that it is reset cyclicly. The reset
     * pulse is  TO10, the normal setting pulse is TO90 if a LOLL instruction
     * has not been performed, in this case it is set by TI05, with a delay
     * of about 130ns. (cpu fo. 96).
     *
     * NOTE: documentation differs at cpu fo. 99 that states:
     *
     * The ACIC1 FF is reset by the TO10 pulse and it is set by the TO901 with
     * the condition PODIB == 1.
     *
     * PODI is the FF which stores the LOLL diagnostic instruction performance
     * causing an increase of the cycle of about 130 ns.
     *
     * In fact, if PODIB == 0 the recycling occurs with the pulse TI05 instead
     * of TO90.
     */
    uint8_t ACIC:1;

    /**
     * Operator Call
     *
     * It commands the switching on of the "Operator call" lamp. It is set with
     * CI87 issued by the LON and LOLL instructions.
     * It is reset with CI88 issued by the LOFF instruction, or by pressing the
     * "CLEAR" button (cpu fo. 96).
     */
    uint8_t ALAM:1;

    /**
     * Jump Condition Verified
     *
     * Reset in the E0 status of the alpha phase, together with AINI, with the
     * CI39 command (cpu fo. 96).
     *
     * Set in the E6 status of the alpha phase of the jump instructions (CI38)
     * if signal DC16 (verified condition) is present (cpu fo. 96).
     */
    uint8_t AVER:1;

    /* The flow chart's <SA00> first-vs-second operand diamond (dwg 14023130)
     * is bit 0 of the SA state code itself: E6 routes operand 1 to ED (bit 0
     * set, via its unconditional CU00) and E7 routes operand 2 to EC (bit 0
     * cleared by CU10 = DI64A0) — see the indexing states in msl-states.c.
     * No separate flag is needed. */

    /**
     * Disable Step By Step
     *
     * Set with CI77 by the INS instruction, reset with CI78 issued by ENS, or
     * with "CLEAR" (cpu fo. 97).
     */
    uint8_t ADIR:1;

    uint8_t RINT:1;

    uint8_t JS1:1;  ///< Console jump condition 1
    uint8_t JS2:1;  ///< Console jump condition 2
    uint8_t JE:1;   ///< JE/AVER jump instruction exectuted
    uint8_t INTE:1; ///< Interruption present

    /* Busy Connector Logic */

    uint8_t PB06:1; ///< Unconditionally stores L106
    uint8_t PB07:1; ///< Unconditionally stores L106
    uint8_t PB26:1; ///< Stores L106 if channel 2 is selected
    uint8_t PB36:1;
    uint8_t PB37:1;

    /**
     * Selection Channel 1
     *
     * Used during the general B phase for command forwarding or condition
     * examination.
     * Unconditionally set by command CE02 which enables the channel selection
     * even if the interested  channels are 2 or 3.
     * When a character transfer in output has been initiated with channel 1,
     * signal PAP4A resets PUC1 at the start of the transfer phase, when the
     * first transfer is done from RO into RA (CE00), unless signal PAR21 had
     * already performed that reset earlier in the status-B0 path. The CPU
     * text explicitly says PAR21 is generated by command CI391. (cpu fo. 235,
     * 237).
     *
     * Note: the above GE docs refers to `PUC2`, however in intermediate block
     * diagram fo. 10, it's shown the real flipflop is `PIC1`, and `PUC2` is
     * derived combinatorially from it.
     */
    uint8_t PIC1:1;

    /**
     * Channel 1 in transfer
     *
     * (cpu fo. 236)
     */
    uint8_t RASI:1;

    /**
     * Channel 2 in transfer
     *
     * (cpu fo. 236)
     */
    uint8_t PUC2:1;

    /**
     * Channel 3 in transfer
     *
     * (cpu fo. 236)
     */
    uint8_t PUC3:1;

    uint8_t PEC1:1;
    uint8_t PEC1_pending:1;

    uint8_t RUF1:1;

    uint8_t URPE:1;
    uint8_t URPU:1;

    /* Cycle Attribution Logic */
    /* ----------------------- */

    /* Asyncronous flip flops */

    /**
     * Asynchronous CPU Cycle Request
     *
     * It is reset with CE18 (enable RIAP) while a cycle is performed
     * for the CPU (RIUC=1). The CPU is thus waiting for the external
     * triggers of the command received.
     * It is set by the clear signal (CAGUF=0) with the signal of
     * command received by the peripheral unit (RBII1=1) with the
     * insertion of the SITE key which frees the waitings (RAITI=1)
     * and finally with the disselection of channel 1 (PU16 = 0)
     * (cpu fo. 114).
     */
    uint8_t RC00:1;

    /**
     * Asynchronous Channel 1 Cycle Request
     *
     * It is set with the OR of the channel 1 request triggers (RAI01)
     * if the executing instruction is not over (RIVEF=1).
     * Also, when the SITE key is inserted during a during a transfer of
     * channel 1 (RAISI2=1).
     * It is reset during a cycle of channel 1 with CE18 (enable RIAP),
     * or at the end of a transfer on channel 1
     * (cpu fo. 114)
     */
    uint8_t RC01:1;

    /**
     * Asynchronous Channel 2 Cycle Request
     *
     * It is set with the trigger LU08 from the integrated reader, or
     * when the SITE key is inserted (RAITI1=1) during the transfers
     * on channel 2.
     *
     * Request from printer do not act on RC02, but are derived from it
     * with an OR (RIMZA).
     *
     * It is reset during a cycle of channel 2 with CE18 (enable RIAP),
     * or at the end of a transfer on channel 2 (cpu fo. 114).
     */
    uint8_t RC02:1;

    /**
     * Asynchronous Channel 3 Cycle Request
     *
     * It is set with the OR of the cycle request triggers relative to
     * channel 3 (RA301=1) if the executing instruction is not over
     * (RIVAF=1) and additional performances of the GE-130 are enabled
     * (FUL4F=1).
     *
     * It is reset during a cycle of channel 3 with CE18 (enable RIAP),
     * also, it is reset when the SITE key is inserted (RAITI=1) during
     * a data transfer on channel 3 (RES36=1), or at the end of transfer
     * on channel 3 (PIC32=0) (cpu fo. 114).
     */
    uint8_t RC03:1;

    /**
     * Synchronous CPU Cycle Request
     *
     * Is conditioned by the signals ALTOF and RAM02.
     *
     * When the FF ALTOF is reset, the cycle requests from the CPU are
     * not serverd, therefore the internal calculation is stopped.
     * This counter consists of the FF RAMO and RAMI and counts with
     * the pulse TO10.
     */
    uint8_t RIA0:1;

    /**
     * Synchronous Channel 1 Cycle Request
     *
     * Transfered from RC01 at pulse TO00 (cpu fo. 114).
     */
    uint8_t RESI:1;

    /**
     * Synchronous Channel 2 Cycle Request
     *
     * Transfered from RC02 at pulse TO00 (cpu fo. 114).
     */
    uint8_t RIA2:1;

    /**
     * Synchronous Channel 3 Cycle Request
     *
     * Transfered from RC03 at pulse TO00 (cpu fo. 114).
     */
    uint8_t RIA3:1;

    /** Selection Check Byte */
    uint8_t RECE:1;

    /** End from controller 1 */
    uint8_t RIG1:1;

    uint8_t RIG3:1;

    /** Rejected Command */
    uint8_t RACI:1;

    /** VICU Support */
    uint8_t RAVI:1;

    uint8_t RT121:1;
    uint8_t RT131:1;

    /**
     * Future state
     *
     * Ad-hoc logic, at the end of the cycle contains the result
     * of the future state network.
     */
    uint8_t future_state;

    /**
     * The current state of the console register rotary switch
     */
    enum ge_console_rotary register_selector;

    /**
     * The current state of the console switches
     */
    struct ge_console_switches console_switches;

    uint8_t step_by_step:1;  ///< Step by step execution @todo replace with signal name

    enum {
        MC_NONE,
        MC_READ,
        MC_WRITE,
    } memory_command;

    uint8_t mem[MEM_SIZE]; ///< The memory of the emulated system

    /** Stored odd-parity bit (1 bit per location) written alongside mem[] */
    uint8_t mem_parity[MEM_SIZE];

    /** 1 once a location has been written; prevents false MEM CHECK on cleared memory */
    uint8_t mem_written[MEM_SIZE];

    /** Installed memory size; 0 is treated as MEM_SIZE (full address space) */
    uint32_t mem_size;

    /** Parity fault flag: set when a READ finds a parity mismatch on a previously-written location */
    uint8_t mem_check;

    /** Invalid-address fault flag: set when rVO >= installed memory size */
    uint8_t inv_add;

    /** Channel-1 peripheral status override for error injection. 0 = report the
     *  default "operation OK" status (0x40); non-zero = report this status byte
     *  instead, so a test/harness can inject a peripheral error/abnormal
     *  condition (e.g. 0x42 sets RO1 -> the EPER "examine" decode sees an error).
     *  Read by CE_chan1_status (msl-commands.c). */
    uint8_t inject_chan1_status;

    struct ge_counting_network counting_network;
    struct ge_ua_controls ua_controls;

    /**
     * The I/O interface for the integrated reader (RI)
     */
    struct ge_integrated_reader integrated_reader;

    /**
     * Integrated printer / console typewriter (channel 2) — pragmatic model.
     * `present` is set only when a printer peripheral is registered (interactive
     * /wasm runs), so the faithful bootstrap channel states are untouched. When
     * present, the channel-2 external-wait (state b8) for a print/typewriter op
     * is completed (B8 -> alpha) instead of hanging, and output bytes are
     * captured into out[]. kbd[] is the operator-keyboard input queue (two-way).
     */
    struct ge_integrated_printer {
        int      present;
        char     out[65536];    /* captured printed chars; per-frame staging only
                                 * (the wasm console drains+clears each frame, the
                                 * CLI reads it at end). Sized for a catch-up
                                 * frame's burst so fast/backgrounded printing
                                 * doesn't overflow before the drain. */
        int      out_len;
        uint8_t  kbd[256];      /* operator keyboard input queue */
        int      kbd_head, kbd_tail;
        /* Channel-2 OUTPUT transfer engine (printer.c): while out_active, the
         * printer requests a channel-2 cycle (RC02) per character so the rSI
         * output state (0x02) drains out_remaining bytes from V4, then ends.
         * out_saved_so preserves the CPU sequencer across the (overlapped)
         * transfer — fsn advances SO on RIA0, which the stolen channel-2 cycles
         * would otherwise clobber; we restore it when the transfer ends. */
        int      out_active;
        int      out_remaining;
        int      out_total;
        int      out_line_mode;
        uint8_t  out_saved_so;
    } integrated_printer;

    /**
     * The I/O interface for the ST3 connector
     */
    struct ge_connector ST3;

    /**
     * The I/O interface for the ST4 connector
     */
    struct ge_connector ST4;

    /**
     * Integrated channel 2 (CAN2) line bundle — shared by the integrated reader
     * (input), the printer/typewriter (output), and the keyboard. Generalises
     * the integrated_reader/connector lines; see channel.h. Phase 3 wires the
     * rSI transfer micro-states to these lines; until then it is scaffolding and
     * the legacy integrated_reader/integrated_printer paths remain authoritative.
     */
    struct ge_channel channel2;

    struct ge_peri *peri;

    /**
     * Shared core for Standard-GE-100 controllers on connectors 3/4 (disk/tape).
     * Owned by connector34.c; NULL until connector34_init(). Opaque here to
     * avoid a header dependency — see connector34.h.
     */
    void *std_core;

    /**
     * Workaround for pulse TO50
     *
     * Currently we first run the common machine logic, then the
     * MSL states. However in certain cases (e.g. display state 00)
     * the common TO50 implementation is conditioned on the activation
     * of the MSL TO50...
     * So, until we figure out a better way of factoring the MSL, let's
     * store here the conditions for the common machine TO50,
     * and delay its excecution to a fake TO50-1 clock pulse.
     */
    uint8_t TO50_did_CI32_or_CI33:1;
};

/// Initialize the emulator
void ge_init(struct ge *ge);

/// Deinitialize the emulator
int ge_deinit(struct ge *ge);

/// Copy a program at the start of memory
int ge_load_program(struct ge *ge, uint8_t *program, uint8_t size);

/// Load a flat image at `origin` (unified-format payload); origin-aware, not
/// size-capped, primes the parity store. Returns 0 on success, -1 on range error.
int ge_load_image(struct ge *ge, const uint8_t *image, size_t size,
                  uint16_t origin);

/// Enter execution at `entry`: seed PO and drop into the alpha (fetch) phase,
/// bypassing the peripheral LOAD bootstrap (direct binary-load path).
void ge_enter(struct ge *ge, uint16_t entry);

/// Store a byte with generated odd parity + mark-written (for the hybrid ALU/SS
/// write paths that write ge->mem[] directly). Keeps parity coherent so a
/// later read doesn't trip a false MEM CHECK.
void ge_mem_store8(struct ge *ge, uint16_t addr, uint8_t val);

/// Seed the eight change/segment-base registers (mem[240+2N]) to identity
/// bases N<<12. Called by ge_clear; re-apply after a direct image load that
/// may have overwritten the 0x00F0-0x00FF window.
void ge_seed_segment_bases(struct ge *ge);

/// Run the emulator
int ge_run(struct ge *ge);

/// Run a single pulse (i.e. a single GE "mastri" clock periods)
int ge_run_pulse(struct ge *ge);

/// Run all GE "mastri" clock periods until next clock cycle
int ge_run_cycle(struct ge *ge);

/// Emulate the press of the "clear" button in the console
void ge_clear(struct ge * ge);

/// Emulate the press of the "load" button in the console
void ge_load(struct ge * ge);

/// Emulate the press of the "load 1" button in the console
void ge_load_1(struct ge * ge);

/// Emulate the press of the "load 2" button in the console
void ge_load_2(struct ge * ge);

/// Emulate the press of the "start" button in the console
void ge_start(struct ge * ge);

typedef void (*on_pulse_cb)(struct ge *);

struct pulse_event {
    on_pulse_cb cb;
    struct pulse_event *next;
};

/* Defined in pulse.c: execute pulse events */
void pulse(struct ge *ge);

struct ge_peri {
    struct ge_peri *next;
    int (*init)(struct ge*, void*);
    int (*on_pulse)(struct ge*, void*);
    int (*on_clock)(struct ge*, void*);
    int (*deinit)(struct ge*, void*);
    void *ctx;
};

int ge_register_peri(struct ge *ge, struct ge_peri *p);

/**
 * Commit the future state
 *
 * Transfers the results of the future state network in the
 * various selectors. For now it's an ad hoc behaviour, not
 * described in detail in the currently available docs.
 */
void fsn_last_clock(struct ge *ge);

void connectors_first_clock(struct ge *ge);

/**
 * The clock period name name
 *
 * Returns the string destribing the clock period
 */
const char *ge_clock_name(enum clock c);

void ge_print_registers_verbose(struct ge *ge);

/**
 * Is the CPU stopped?
 *
 * ALTO is the hardware stop flip-flop (cpu fo.97/98): it gates RIA0, so while
 * it is set the CPU is attributed no work cycles.  The delay line keeps
 * turning and the channels keep running -- a halted GE-120 still drives its
 * panel -- so a front end that models the whole machine (the wasm console)
 * should keep cycling and simply watch this; a batch front end that only
 * cares about the program uses it to end the run.
 *
 * There used to be a separate `halted` field alongside it.  It was redundant
 * and disagreed with ALTO in both directions: set at init while ALTO was
 * clear, cleared by ge_clear() while ALTO was set, and -- the actual bug --
 * left clear by every operator stop (ACOV/ACON, PAPA, PATE), so the CLI run
 * loop kept spinning against a frozen CPU.
 */
static inline uint8_t ge_halted(const struct ge *ge) { return ge->ALTO; }

/** Report the strapped configuration and every level it produces (LOG_DEBUG). */
void ge_log_options(struct ge *ge);

#endif /* GE_H */
