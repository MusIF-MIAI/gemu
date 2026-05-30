#include "utest.h"

#include "../ge.h"
#include "../alu_cc.h"
#include "../opcodes.h"
#include "../alu_logic.h"
#include "../alu_dec.h"

/*
 * Instruction-execution tests (Wave 5).
 *
 * These exercise the full fetch -> decode -> operand-fetch -> beta-execute
 * -> alpha pipeline for the PM/SI immediate-format data ops, which are
 * executed in the beta phase (state 64/65) via the alu_* helpers.
 *
 * Operand layout at beta for these ops: V1 = address, L1 = immediate byte.
 */

/* Run whole cycles until the machine returns to alpha (e2/e3) right after a
 * beta phase (64/65) — i.e. one instruction has completed. */
static void run_one(struct ge *g)
{
    int last = -1;
    for (int i = 0; i < 40; i++) {
        ge_run_cycle(g);
        if ((g->rSO == 0xe2 || g->rSO == 0xe3) &&
            (last == 0x64 || last == 0x65))
            return;
        last = g->rSO;
    }
}

/* Run whole cycles until the machine returns to alpha (e2/e3) right after
 * the SS-format operand-fetch micro-loop (state E7) — i.e. one SS instruction
 * has completed via Mechanism B. */
static void run_one_ss(struct ge *g)
{
    int last = -1;
    for (int i = 0; i < 40; i++) {
        ge_run_cycle(g);
        /* Completion lands in alpha from the second-operand fetch (E7, absolute
         * source) or from the indexing micro-cycle's EF|EE (0xee, modified
         * source). */
        if ((g->rSO == 0xe2 || g->rSO == 0xe3) &&
            (last == 0xe7 || last == 0xee))
            return;
        last = g->rSO;
    }
}

static void setup(struct ge *g, uint8_t *prog, int n)
{
    ge_init(g);
    ge_load_program(g, prog, (uint8_t)n);
    ge_clear(g);
    ge_start(g);
}

UTEST(exec, mvi_stores_immediate)
{
    uint8_t prog[] = { MVI_OPCODE, 0xAB, 0x00, 0x50 };
    struct ge g; setup(&g, prog, sizeof(prog));
    run_one(&g);
    ASSERT_EQ(g.mem[0x50], 0xAB);
    ASSERT_EQ(g.rPO, 0x0004);            /* PO advanced past the 4-byte instr */
    ASSERT_TRUE(g.rSO == 0xe2 || g.rSO == 0xe3);
}

UTEST(exec, ni_ands_immediate)
{
    uint8_t prog[] = { NI_OPCODE, 0x0F, 0x00, 0x50 };
    struct ge g; setup(&g, prog, sizeof(prog));
    g.mem[0x50] = 0xF3;
    run_one(&g);
    ASSERT_EQ(g.mem[0x50], 0x03);        /* 0xF3 & 0x0F */
}

UTEST(exec, xi_xors_immediate)
{
    uint8_t prog[] = { XI_OPCODE, 0x0F, 0x00, 0x50 };
    struct ge g; setup(&g, prog, sizeof(prog));
    g.mem[0x50] = 0xFF;
    run_one(&g);
    ASSERT_EQ(g.mem[0x50], 0xF0);        /* 0xFF ^ 0x0F */
}

/* CI (0x96) is OR Immediate (deck step 0x32): mem |= K. */
UTEST(exec, ci_or_immediate)
{
    uint8_t prog[] = { CI_OPCODE, 0xAA, 0x00, 0x50 };
    struct ge g; setup(&g, prog, sizeof(prog));
    g.mem[0x50] = 0x55;
    run_one(&g);
    ASSERT_EQ(g.mem[0x50], 0xFF);              /* 0x55 | 0xAA = 0xFF */
    ASSERT_EQ(alu_get_cc(&g), 3);              /* result non-zero -> cc 3 */
}

/* CMI (0x95) is Compare Immediate (the comparison op). */
UTEST(exec, cmi_compare_equal_sets_cc2)
{
    uint8_t prog[] = { CMI_OPCODE, 0x42, 0x00, 0x50 };
    struct ge g; setup(&g, prog, sizeof(prog));
    g.mem[0x50] = 0x42;
    run_one(&g);
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_EQUAL);   /* equal -> cc 2 */
    ASSERT_EQ(g.mem[0x50], 0x42);              /* CMI does not modify memory */
}

UTEST(exec, cmi_compare_low_sets_cc1)
{
    uint8_t prog[] = { CMI_OPCODE, 0x42, 0x00, 0x50 };
    struct ge g; setup(&g, prog, sizeof(prog));
    g.mem[0x50] = 0x10;                        /* mem < immediate */
    run_one(&g);
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_LOW);     /* first < second -> cc 1 */
}

/* Change registers are memory-mapped 16-bit at 240 + N*2.  The register-code
 * aux char has the form 1XXX0000 (bits 4-6 = N), so register 4 = 0xC0 ->
 * address 248 (0xF8). */
#define REG4 0xF8

UTEST(exec, lr_loads_register_from_memory)
{
    uint8_t prog[] = { LR_OPCODE, 0xC0, 0x00, 0x60 };  /* LD reg4 <- mem[0x60] */
    struct ge g; setup(&g, prog, sizeof(prog));
    g.mem[0x60] = 0x12; g.mem[0x61] = 0x34;
    run_one(&g);
    ASSERT_EQ(g.mem[REG4], 0x12);
    ASSERT_EQ(g.mem[REG4 + 1], 0x34);
}

UTEST(exec, str_stores_register_to_memory)
{
    uint8_t prog[] = { STR_OPCODE, 0xC0, 0x00, 0x60 };  /* STR reg4 -> mem[0x60] */
    struct ge g; setup(&g, prog, sizeof(prog));
    g.mem[REG4] = 0xAB; g.mem[REG4 + 1] = 0xCD;
    run_one(&g);
    ASSERT_EQ(g.mem[0x60], 0xAB);
    ASSERT_EQ(g.mem[0x61], 0xCD);
}

UTEST(exec, la_loads_address)
{
    /* LA loads the EFFECTIVE address. Bits 14-12 of an address select an
     * index register, so use 0x0234 (no index) to load it verbatim. */
    uint8_t prog[] = { LA_OPCODE, 0xC0, 0x02, 0x34 };   /* LA reg4 <- 0x0234 */
    struct ge g; setup(&g, prog, sizeof(prog));
    run_one(&g);
    ASSERT_EQ(g.mem[REG4], 0x02);
    ASSERT_EQ(g.mem[REG4 + 1], 0x34);
}

UTEST(exec, amr_adds_memory_to_register)
{
    uint8_t prog[] = { AMR_OPCODE, 0xC0, 0x00, 0x60 };  /* reg4 += mem[0x60] */
    struct ge g; setup(&g, prog, sizeof(prog));
    g.mem[REG4] = 0x00; g.mem[REG4 + 1] = 0x10;         /* reg4 = 0x0010 */
    g.mem[0x60] = 0x00; g.mem[0x61] = 0x05;             /* operand = 0x0005 */
    run_one(&g);
    ASSERT_EQ(g.mem[REG4], 0x00);
    ASSERT_EQ(g.mem[REG4 + 1], 0x15);                   /* 0x10 + 0x05 */
    ASSERT_EQ(alu_get_cc(&g), 1);                       /* nonzero, no overflow */
}

UTEST(exec, cmr_compare_equal)
{
    uint8_t prog[] = { CMR_OPCODE, 0xC0, 0x00, 0x60 };  /* cmp reg4 : mem[0x60] */
    struct ge g; setup(&g, prog, sizeof(prog));
    g.mem[REG4] = 0x11; g.mem[REG4 + 1] = 0x22;
    g.mem[0x60] = 0x11; g.mem[0x61] = 0x22;
    run_one(&g);
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_EQUAL);
}

/* -----------------------------------------------------------------------
 * SS (Storage-to-Storage) instruction tests (Wave 5 / Mechanism B).
 *
 * SS instruction format: opcode  LL  A1hi A1lo  A2hi A2lo
 *   LL  = length byte
 *   A1  = destination (op1) address
 *   A2  = source (op2) address
 *
 * After one instruction via run_one_ss:
 *   - SO is alpha (e2/e3)
 *   - PO = 6 (past the 6-byte instruction)
 * ----------------------------------------------------------------------- */

/* MVC – Move Characters: copies len bytes from src to dst. */
UTEST(exec, mvc_copies_bytes)
{
    /* MVC: opcode=0xd2, LL=0x02 (3 bytes), dst=0x0050, src=0x0060 */
    uint8_t prog[] = { MVC_OPCODE, 0x02, 0x00, 0x50, 0x00, 0x60 };
    struct ge g; setup(&g, prog, sizeof(prog));
    g.mem[0x60] = 0xAA;
    g.mem[0x61] = 0xBB;
    g.mem[0x62] = 0xCC;
    run_one_ss(&g);
    ASSERT_EQ(g.mem[0x50], 0xAA);
    ASSERT_EQ(g.mem[0x51], 0xBB);
    ASSERT_EQ(g.mem[0x52], 0xCC);
    ASSERT_EQ(g.rPO, 0x0006);          /* PO advanced past the 6-byte instr */
    ASSERT_TRUE(g.rSO == 0xe2 || g.rSO == 0xe3);
}

/* NC – AND Characters: ANDs len bytes at dst with src, result to dst. */
UTEST(exec, nc_ands_bytes)
{
    /* NC: opcode=0xd4, LL=0x01 (2 bytes), dst=0x0050, src=0x0060 */
    uint8_t prog[] = { NC_OPCODE, 0x01, 0x00, 0x50, 0x00, 0x60 };
    struct ge g; setup(&g, prog, sizeof(prog));
    g.mem[0x50] = 0xFF; g.mem[0x51] = 0xAA;
    g.mem[0x60] = 0x0F; g.mem[0x61] = 0x55;
    run_one_ss(&g);
    ASSERT_EQ(g.mem[0x50], 0x0F);   /* 0xFF & 0x0F */
    ASSERT_EQ(g.mem[0x51], 0x00);   /* 0xAA & 0x55 */
    ASSERT_EQ(g.rPO, 0x0006);
    ASSERT_TRUE(g.rSO == 0xe2 || g.rSO == 0xe3);
}

/* CMC – Compare Characters: sets CC based on byte comparison. */
UTEST(exec, cmc_equal_sets_cc2)
{
    /* CMC: opcode=0xd5, LL=0x02 (3 bytes), op1=0x0050, op2=0x0060 */
    uint8_t prog[] = { CMC_OPCODE, 0x02, 0x00, 0x50, 0x00, 0x60 };
    struct ge g; setup(&g, prog, sizeof(prog));
    g.mem[0x50] = 0x41; g.mem[0x51] = 0x42; g.mem[0x52] = 0x43;
    g.mem[0x60] = 0x41; g.mem[0x61] = 0x42; g.mem[0x62] = 0x43;
    run_one_ss(&g);
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_EQUAL);
    ASSERT_EQ(g.rPO, 0x0006);
    ASSERT_TRUE(g.rSO == 0xe2 || g.rSO == 0xe3);
}

UTEST(exec, cmc_low_sets_cc1)
{
    /* CMC: op1 < op2 should set CC=1 (LOW) */
    uint8_t prog[] = { CMC_OPCODE, 0x00, 0x00, 0x50, 0x00, 0x60 };
    struct ge g; setup(&g, prog, sizeof(prog));
    g.mem[0x50] = 0x10;   /* first operand: smaller */
    g.mem[0x60] = 0x20;   /* second operand: larger */
    run_one_ss(&g);
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_LOW);
}

/* AP – Add Packed decimal.
 *
 * Length byte layout (two-nibble, S/360-style):
 *   high nibble = l1 (destination field length - 1)
 *   low  nibble = l2 (source field length - 1)
 *
 * AP 0x10: l1=1+1=2 bytes (op1), l2=0+1=1 byte (op2).
 * op1 at 0x0070 (rightmost byte), op2 at 0x0080 (rightmost byte).
 * op1 = 0x01 0C (+1, 2-byte packed), op2 = 0x02 C (+2, 1-byte packed).
 * Expected result: +3, stored at 0x006f-0x0070.
 */
UTEST(exec, ap_adds_packed_decimal)
{
    /* AP: opcode=0xea, LL=0x10, dst=0x0070, src=0x0080 */
    uint8_t prog[] = { AP_OPCODE, 0x10, 0x00, 0x70, 0x00, 0x80 };
    struct ge g; setup(&g, prog, sizeof(prog));
    /* op1: 2-byte packed +1 at addresses 0x6f-0x70 (rightmost = 0x70) */
    g.mem[0x6f] = 0x00;
    g.mem[0x70] = 0x1C;   /* +1 */
    /* op2: 1-byte packed +2 at address 0x80 (rightmost = 0x80) */
    g.mem[0x80] = 0x2C;   /* +2 */
    run_one_ss(&g);
    /* result: +3 at 0x6f-0x70 */
    ASSERT_EQ(g.mem[0x6f], 0x00);
    ASSERT_EQ(g.mem[0x70], 0x3C);   /* +3 */
    ASSERT_EQ(g.rPO, 0x0006);
    ASSERT_TRUE(g.rSO == 0xe2 || g.rSO == 0xe3);
}

/* -----------------------------------------------------------------------
 * Wave 6: SS ops wired in from existing ALU helpers (AB/SB/AD/SD/MVQ/CMQ).
 *
 * Two-length ops (AB/SB/AD/SD) take a full byte-count per operand: the LL
 * byte's high nibble = L1-1, low nibble = L2-1.  AB/SB are binary, AD/SD are
 * unsigned zoned decimal (zones ignored, result zone cleared).
 * MVQ/CMQ are single-length (len = LL+1) quartet ops on the digit nibbles.
 * ----------------------------------------------------------------------- */

/* AB – Add Binary: 1-byte op1 += 1-byte op2 (LL=0x00 -> L1=L2=1). */
UTEST(exec, ab_adds_binary)
{
    uint8_t prog[] = { AB_OPCODE, 0x00, 0x00, 0x70, 0x00, 0x80 };
    struct ge g; setup(&g, prog, sizeof(prog));
    g.mem[0x70] = 0x05;
    g.mem[0x80] = 0x03;
    run_one_ss(&g);
    ASSERT_EQ(g.mem[0x70], 0x08);
    ASSERT_EQ(alu_get_cc(&g), 1);              /* nonzero, no overflow */
    ASSERT_EQ(g.rPO, 0x0006);
    ASSERT_TRUE(g.rSO == 0xe2 || g.rSO == 0xe3);
}

/* SB – Subtract Binary: positive result sets cc=3. */
UTEST(exec, sb_subtracts_binary)
{
    uint8_t prog[] = { SB_OPCODE, 0x00, 0x00, 0x70, 0x00, 0x80 };
    struct ge g; setup(&g, prog, sizeof(prog));
    g.mem[0x70] = 0x08;
    g.mem[0x80] = 0x03;
    run_one_ss(&g);
    ASSERT_EQ(g.mem[0x70], 0x05);
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_POS);     /* result > 0 */
}

/* AD – Add Decimal (zoned): digits add, result zone cleared to 0. */
UTEST(exec, ad_adds_unpacked_decimal)
{
    uint8_t prog[] = { AD_OPCODE, 0x00, 0x00, 0x70, 0x00, 0x80 };
    struct ge g; setup(&g, prog, sizeof(prog));
    g.mem[0x70] = 0xF5;   /* zoned '5' */
    g.mem[0x80] = 0xF3;   /* zoned '3' */
    run_one_ss(&g);
    ASSERT_EQ(g.mem[0x70], 0xF8);              /* digit 8, first-operand zone (F) preserved */
    ASSERT_EQ(alu_get_cc(&g), 1);              /* no overflow, nonzero */
}

/* SD – Subtract Decimal (zoned). */
UTEST(exec, sd_subtracts_unpacked_decimal)
{
    uint8_t prog[] = { SD_OPCODE, 0x00, 0x00, 0x70, 0x00, 0x80 };
    struct ge g; setup(&g, prog, sizeof(prog));
    g.mem[0x70] = 0xF8;   /* zoned '8' */
    g.mem[0x80] = 0xF3;   /* zoned '3' */
    run_one_ss(&g);
    ASSERT_EQ(g.mem[0x70], 0xF5);              /* digit 5, first-operand zone (F) preserved */
    ASSERT_EQ(alu_get_cc(&g), 3);              /* result > 0 */
}

/* MVQ – Move Quartets: copy digit nibble from src, preserve dst zone. */
UTEST(exec, mvq_moves_digit_preserving_zone)
{
    uint8_t prog[] = { MVQ_OPCODE, 0x00, 0x00, 0x70, 0x00, 0x80 };
    struct ge g; setup(&g, prog, sizeof(prog));
    g.mem[0x70] = 0x50;   /* dst zone 5, digit 0 */
    g.mem[0x80] = 0x73;   /* src zone 7, digit 3 */
    run_one_ss(&g);
    ASSERT_EQ(g.mem[0x70], 0x53);              /* dst zone kept, digit from src */
    ASSERT_EQ(alu_get_cc(&g), 1);              /* transferred digit nonzero */
}

/* CMQ – Compare Quartets: 2-byte fields, op1 > op2 sets cc=3. */
UTEST(exec, cmq_compares_quartets_high)
{
    uint8_t prog[] = { CMQ_OPCODE, 0x01, 0x00, 0x71, 0x00, 0x81 };
    struct ge g; setup(&g, prog, sizeof(prog));
    g.mem[0x70] = 0xF1; g.mem[0x71] = 0xF5;    /* op1 digits "1 5" */
    g.mem[0x80] = 0xF1; g.mem[0x81] = 0xF3;    /* op2 digits "1 3" */
    run_one_ss(&g);
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_HIGH);    /* 15 > 13 */
    ASSERT_EQ(g.mem[0x70], 0xF1);              /* CMQ does not modify memory */
    ASSERT_EQ(g.mem[0x71], 0xF5);
}

/* JRT – Jump Return (op 0x41): branch-and-link. Deposits the return address
 * (the subsequent instruction) into index register 7 (mem 254/255) and jumps.
 * CPU[4] sec.5.5.6.2 / 5.6.5.1. This is the call mechanism for the C ABI. */
UTEST(exec, jrt_links_and_jumps)
{
    /* JRT ,0x000A with mask 0xF0 (unconditional). Subsequent instr = 0x0004. */
    uint8_t prog[] = { JRT_OPCODE, 0xF0, 0x00, 0x0A };
    struct ge g; setup(&g, prog, sizeof(prog));
    run_one(&g);
    ASSERT_EQ(g.rPO, 0x000A);              /* jumped to target */
    ASSERT_EQ(g.mem[254], 0x00);           /* reg7 hi = return addr hi */
    ASSERT_EQ(g.mem[255], 0x04);           /* reg7 lo = return addr (0x0004) */
}

/* Conditional JRT must still deposit the link even when the jump is NOT taken
 * (mask 0x00 -> never jumps; link reserved per sec.5.6.5.1). */
UTEST(exec, jrt_links_even_when_not_taken)
{
    uint8_t prog[] = { JRT_OPCODE, 0x00, 0x00, 0x0A };
    struct ge g; setup(&g, prog, sizeof(prog));
    run_one(&g);
    ASSERT_EQ(g.rPO, 0x0004);              /* fell through (no jump) */
    ASSERT_EQ(g.mem[254], 0x00);
    ASSERT_EQ(g.mem[255], 0x04);           /* link still deposited */
}

/* -----------------------------------------------------------------------
 * Address bit 15 = absolute(0)/modified(1) flag (CPU[4] §2.5; flow chart dwg
 * 14023130). A modified field (bit 15 set) selects change register N from bits
 * 12-14 and resolves to change_register[N] + (bits 0-11 displacement) via the
 * indexing micro-cycle ED|EC -> EF|EE. An absolute field (bit 15 clear) is used
 * directly. These tests exercise the indexing cycle for a single-address (PM)
 * op, an SS destination, and an SS op with both operands modified.
 * ----------------------------------------------------------------------- */

/* PM single-address op, modified address: MVI imm into disp(N).
 * Field 0xB010 = modified(bit15) | modifier 3 | disp 0x010. reg3 := 0x0400, so
 * EA = 0x0400 + 0x010 = 0x0410. */
UTEST(exec, index_pm_modified_resolves_changereg_plus_disp)
{
    uint8_t prog[] = { MVI_OPCODE, 0xAB, 0xB0, 0x10 };  /* MVI 0xAB, 0x010(3) */
    struct ge g; setup(&g, prog, sizeof(prog));
    g.mem[0xF6] = 0x04; g.mem[0xF7] = 0x00;   /* change register 3 = 0x0400 */
    run_one(&g);
    ASSERT_EQ(g.mem[0x0410], 0xAB);            /* wrote to chgreg[3] + disp */
    ASSERT_EQ(g.mem[0x3010], 0x00);            /* NOT the identity-base address */
    ASSERT_EQ(g.rPO, 0x0004);
    ASSERT_TRUE(g.rSO == 0xe2 || g.rSO == 0xe3);
}

/* SS destination modified, source absolute. Dest field 0xD004 = modified |
 * modifier 5 | disp 4; reg5 = 0x0600 -> EA 0x0604. Source 0x0080 is absolute
 * (bit 15 clear) -> used directly. */
UTEST(exec, ss_dest_modified_indexes_changereg)
{
    uint8_t prog[] = { MVC_OPCODE, 0x00, 0xD0, 0x04, 0x00, 0x80 }; /* MVC 1, 0x004(5), 0x80 */
    struct ge g; setup(&g, prog, sizeof(prog));
    g.mem[0xFA] = 0x06; g.mem[0xFB] = 0x00;   /* change register 5 = 0x0600 */
    g.mem[0x80] = 0xAB;                        /* source byte (absolute) */
    run_one_ss(&g);
    ASSERT_EQ(g.mem[0x0604], 0xAB);            /* wrote to chgreg[5] + disp */
    ASSERT_EQ(g.mem[0x5004], 0x00);            /* NOT the raw (absolute) field */
}

/* Modified jump through a change register = the C ABI's subroutine return
 * (`JU 0x000(7)` after JRT deposited the link in reg 7). Field 0xF000 =
 * modified | modifier 7 | disp 0. reg7 := 0x0420, so the jump resolves the
 * target via the indexing micro-cycle and lands at PO = 0x0420. */
UTEST(exec, modified_jump_via_changereg_returns)
{
    uint8_t prog[] = { JU_OPCODE, 0xF0, 0xF0, 0x00 };   /* JU 0x000(7) */
    struct ge g; setup(&g, prog, sizeof(prog));
    g.mem[254] = 0x04; g.mem[255] = 0x20;               /* change register 7 = 0x0420 */
    run_one(&g);
    ASSERT_EQ(g.rPO, 0x0420);                            /* jumped to chgreg[7] + disp */
    ASSERT_TRUE(g.rSO == 0xe2 || g.rSO == 0xe3);
}

/* SS with BOTH operands modified: dst disp(5), src disp(2). reg5 = 0x0600,
 * reg2 = 0x0700. dst EA = 0x0604, src EA = 0x0708. */
UTEST(exec, ss_both_operands_modified)
{
    uint8_t prog[] = { MVC_OPCODE, 0x00, 0xD0, 0x04, 0xA0, 0x08 }; /* MVC 1, 0x004(5), 0x008(2) */
    struct ge g; setup(&g, prog, sizeof(prog));
    g.mem[0xFA] = 0x06; g.mem[0xFB] = 0x00;   /* change register 5 = 0x0600 */
    g.mem[0xF4] = 0x07; g.mem[0xF5] = 0x00;   /* change register 2 = 0x0700 */
    g.mem[0x0708] = 0x5A;                      /* source byte */
    run_one_ss(&g);
    ASSERT_EQ(g.mem[0x0604], 0x5A);            /* dst (chgreg[5]+4) <- src (chgreg[2]+8) */
    ASSERT_EQ(g.rPO, 0x0006);
    ASSERT_TRUE(g.rSO == 0xe2 || g.rSO == 0xe3);
}
