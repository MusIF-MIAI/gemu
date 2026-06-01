/*
 * tests/reader_signals.c - COCA channel-1 reader pin behaviour.
 *
 * Phase 1: the CPU->reader strobe lines (TU00N / TU03N / REGEN) are driven by
 *          the command hooks.
 * Phase 2: a CPU mode-select READ command latches the read mode (COCON), while
 *          the plain "read unchanged" (0x40) leaves the mode alone — which is
 *          what keeps the bootstrap on its harness/default mode.
 */

#include "utest.h"

#include "../ge.h"
#include "../reader.h"
#include "../transcode.h"
#include "../log.h"
#include "../signals.h"

/* Phase 1 — TU00N / TU03N / REGEN strobe lines. */
UTEST(reader_signals, tu_strobes_and_regen)
{
    struct ge g;
    ge_init(&g);
    ge_log_set_active_types(0);

    g.rRE = 0x40;                       /* "read unchanged" */
    reader_send_tu00(&g);               /* CE10 / send TU20 of channel 1 */
    ASSERT_EQ((int)g.integrated_reader.tu00, 1);   /* TU00N read-strobe line */

    reader_send_tu10(&g);               /* CE09 / send TU10 of channel 1 */
    ASSERT_EQ((int)g.integrated_reader.tu03, 1);   /* TU03N feed/advance line */

    ge_clear(&g);                       /* REGEN: general clear */
    ASSERT_EQ((int)g.integrated_reader.regen, 1);
    ASSERT_EQ((int)g.integrated_reader.tu00, 0);   /* command lines cleared */
}

/* Phase 2 — the plain read (0x40) must NOT engage the CPU-selected mode. */
UTEST(reader_signals, plain_read_keeps_mode)
{
    struct ge g;
    ge_init(&g);
    ge_log_set_active_types(0);
    ge_clear(&g);

    g.rRE = 0x40;
    reader_send_tu00(&g);
    ASSERT_EQ((int)g.integrated_reader.active_valid, 0);
}

/* Phase 2 — read-binary (0x20) latches active_mode = TC_BINARY via COCON. */
UTEST(reader_signals, read_binary_selects_mode)
{
    struct ge g;
    ge_init(&g);
    ge_log_set_active_types(0);
    ge_clear(&g);

    g.rRE = 0x20;                       /* "read binary" */
    reader_send_tu00(&g);
    ASSERT_EQ((int)g.integrated_reader.active_valid, 1);
    ASSERT_EQ((int)g.integrated_reader.cocon, 1);
    ASSERT_EQ((int)g.integrated_reader.mode_debi, 1);
    ASSERT_EQ((int)g.integrated_reader.active_mode, (int)TC_BINARY);
}

/* Phase 2 — read-normal (0x21) latches active_mode = TC_NORMAL. */
UTEST(reader_signals, read_normal_selects_mode)
{
    struct ge g;
    ge_init(&g);
    ge_log_set_active_types(0);
    ge_clear(&g);

    g.rRE = 0x21;                       /* "read normal i" */
    reader_send_tu00(&g);
    ASSERT_EQ((int)g.integrated_reader.active_valid, 1);
    ASSERT_EQ((int)g.integrated_reader.mode_n001, 1);
    ASSERT_EQ((int)g.integrated_reader.active_mode, (int)TC_NORMAL);
}

/* Phase 3 — LENON ("not operable") inhibits the TU03 feed strobe; an operable
 * reader feeds normally. */
UTEST(reader_signals, lenon_inhibits_feed)
{
    struct ge g;
    ge_init(&g);
    ge_log_set_active_types(0);
    ge_clear(&g);

    /* Operable (lenon=0): CE09 raises the feed line. */
    g.integrated_reader.lenon = 0;
    reader_send_tu10(&g);
    ASSERT_EQ((int)g.integrated_reader.tu03, 1);

    /* Not operable (lenon=1): the feed strobe is suppressed. */
    g.integrated_reader.tu03 = 0;
    g.integrated_reader.lenon = 1;
    reader_send_tu10(&g);
    ASSERT_EQ((int)g.integrated_reader.tu03, 0);
}

/* Phase 4.5 — end-of-transfer completion is not raised directly by the
 * peripheral helper; it is latched onto PEC1 at TO50, resets RASI at TO70,
 * and clears again at TO89. */
UTEST(reader_signals, pec1_latches_on_to50)
{
    struct ge g;
    ge_init(&g);
    ge_log_set_active_types(0);
    ge_clear(&g);

    g.RASI = 1;
    reader_setup_to_send(&g, 0xAA, 1);

    ASSERT_EQ((int)g.RIG1, 1);
    ASSERT_EQ((int)g.PEC1, 0);
    ASSERT_EQ((int)g.PEC1_pending, 1);

    g.current_clock = TO50;
    pulse(&g);
    ASSERT_EQ((int)g.PEC1, 1);
    ASSERT_EQ((int)g.PEC1_pending, 0);

    g.current_clock = TO70;
    pulse(&g);
    ASSERT_EQ((int)g.RASI, 0);

    g.current_clock = TO89;
    pulse(&g);
    ASSERT_EQ((int)g.PEC1, 0);
}

/* Phase 5 — RL1U1 is the channel-1 length terminal-count decode: L1 all-ones.
 * This is the decode that the RENIA end-of-transfer equation consumes (gated by
 * L204, the order-block length-counted bit). */
UTEST(reader_signals, rl1u1_terminal_decode)
{
    struct ge g;
    ge_init(&g);
    ge_log_set_active_types(0);
    ge_clear(&g);

    g.rL1 = 0x0080;                     /* order length (e.g. bootstrap): not terminal */
    ASSERT_EQ((int)RL1U1(&g), 0);

    g.rL1 = 0x007F;                     /* one short of terminal */
    ASSERT_EQ((int)RL1U1(&g), 0);

    g.rL1 = 0x00FF;                     /* all ones: terminal count reached */
    ASSERT_EQ((int)RL1U1(&g), 1);

    g.rL1 = 0x12FF;                     /* only the low byte matters */
    ASSERT_EQ((int)RL1U1(&g), 1);
}
