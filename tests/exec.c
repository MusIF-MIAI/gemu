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
        if ((g->rSO == 0xe2 || g->rSO == 0xe3) && last == 0xe7)
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

UTEST(exec, ci_compare_equal_sets_cc2)
{
    uint8_t prog[] = { CI_OPCODE, 0x42, 0x00, 0x50 };
    struct ge g; setup(&g, prog, sizeof(prog));
    g.mem[0x50] = 0x42;
    run_one(&g);
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_EQUAL);   /* equal -> cc 2 */
    ASSERT_EQ(g.mem[0x50], 0x42);              /* CI does not modify memory */
}

UTEST(exec, ci_compare_low_sets_cc1)
{
    uint8_t prog[] = { CI_OPCODE, 0x42, 0x00, 0x50 };
    struct ge g; setup(&g, prog, sizeof(prog));
    g.mem[0x50] = 0x10;                        /* mem < immediate */
    run_one(&g);
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_LOW);     /* first < second -> cc 1 */
}

/* Change registers are memory-mapped 16-bit at 240 + N*2.  Register-code
 * aux char 0x0C selects register 4 -> address 248 (0xF8). */
#define REG4 0xF8

UTEST(exec, lr_loads_register_from_memory)
{
    uint8_t prog[] = { LR_OPCODE, 0x0C, 0x00, 0x60 };  /* LD reg4 <- mem[0x60] */
    struct ge g; setup(&g, prog, sizeof(prog));
    g.mem[0x60] = 0x12; g.mem[0x61] = 0x34;
    run_one(&g);
    ASSERT_EQ(g.mem[REG4], 0x12);
    ASSERT_EQ(g.mem[REG4 + 1], 0x34);
}

UTEST(exec, str_stores_register_to_memory)
{
    uint8_t prog[] = { STR_OPCODE, 0x0C, 0x00, 0x60 };  /* STR reg4 -> mem[0x60] */
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
    uint8_t prog[] = { LA_OPCODE, 0x0C, 0x02, 0x34 };   /* LA reg4 <- 0x0234 */
    struct ge g; setup(&g, prog, sizeof(prog));
    run_one(&g);
    ASSERT_EQ(g.mem[REG4], 0x02);
    ASSERT_EQ(g.mem[REG4 + 1], 0x34);
}

UTEST(exec, amr_adds_memory_to_register)
{
    uint8_t prog[] = { AMR_OPCODE, 0x0C, 0x00, 0x60 };  /* reg4 += mem[0x60] */
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
    uint8_t prog[] = { CMR_OPCODE, 0x0C, 0x00, 0x60 };  /* cmp reg4 : mem[0x60] */
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
