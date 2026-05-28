/**
 * @file  tests/memory.c
 * @brief Unit tests for the memory subsystem: round-trip, parity check, invalid address.
 *
 * Drive memory cycles through the public pulse() interface.
 * READ happens in on_TO50 (current_clock = TO50).
 * WRITE happens in on_TO65 (current_clock = TO65).
 *
 * No UTEST_MAIN here — tests/main.c provides it.
 */

#include "utest.h"
#include "../ge.h"

/* ------------------------------------------------------------------ */
/* (a) Round-trip: write 0x5A to address 0x100, read it back.         */
/*     Expect rRO == 0x5A, mem_check == 0, inv_add == 0.              */
/* ------------------------------------------------------------------ */
UTEST(memory, round_trip)
{
    struct ge g;
    ge_init(&g);

    /* WRITE cycle */
    g.rVO = 0x100;
    g.rRO = 0x5A;
    g.memory_command = MC_WRITE;
    g.current_clock  = TO65;
    pulse(&g);

    ASSERT_EQ(g.mem[0x100], (uint8_t)0x5A);
    ASSERT_EQ(g.mem_written[0x100], (uint8_t)1);
    ASSERT_EQ(g.inv_add,  (uint8_t)0);
    ASSERT_EQ(g.mem_check,(uint8_t)0);

    /* READ cycle */
    g.rVO = 0x100;
    g.rRO = 0x00; /* clear so we can verify the read fills it */
    g.memory_command = MC_READ;
    g.current_clock  = TO50;
    pulse(&g);

    ASSERT_EQ(g.rRO,      (uint8_t)0x5A);
    ASSERT_EQ(g.mem_check,(uint8_t)0);
    ASSERT_EQ(g.inv_add,  (uint8_t)0);
}

/* ------------------------------------------------------------------ */
/* (b) Parity fault: corrupt a written location then read it.         */
/*     Expect mem_check == 1.                                          */
/* ------------------------------------------------------------------ */
UTEST(memory, parity_fault)
{
    struct ge g;
    ge_init(&g);

    /* Write a known byte */
    g.rVO = 0x100;
    g.rRO = 0xA5;
    g.memory_command = MC_WRITE;
    g.current_clock  = TO65;
    pulse(&g);

    ASSERT_EQ(g.mem_written[0x100], (uint8_t)1);
    ASSERT_EQ(g.mem_check, (uint8_t)0);

    /* Corrupt the stored byte so parity no longer matches */
    g.mem[0x100] ^= 0x01;

    /* READ cycle — should detect parity mismatch */
    g.rVO = 0x100;
    g.memory_command = MC_READ;
    g.current_clock  = TO50;
    pulse(&g);

    ASSERT_EQ(g.mem_check, (uint8_t)1);
    ASSERT_EQ(g.inv_add,   (uint8_t)0);
}

/* ------------------------------------------------------------------ */
/* (c) Invalid address: address above installed memory raises inv_add.*/
/* ------------------------------------------------------------------ */
UTEST(memory, invalid_address)
{
    struct ge g;
    ge_init(&g);

    /* Restrict installed memory to 4 KB */
    g.mem_size = 0x1000;

    /* WRITE to an address well outside installed memory */
    g.rVO = 0x2000;
    g.rRO = 0xBE;
    g.memory_command = MC_WRITE;
    g.current_clock  = TO65;
    pulse(&g);

    ASSERT_EQ(g.inv_add,  (uint8_t)1);
    /* The store must NOT have happened */
    ASSERT_EQ(g.mem[0x2000 & 0xFFFF], (uint8_t)0);

    /* Reset fault flag, then try a READ at the same out-of-range address */
    g.inv_add = 0;
    g.memory_command = MC_READ;
    g.current_clock  = TO50;
    pulse(&g);

    ASSERT_EQ(g.inv_add, (uint8_t)1);
}
