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

/* Phase 3 — LENON (manual) inhibits the TU03 feed strobe; a non-manual reader
 * feeds normally. (Fault injection: a reader left in manual mode.) */
UTEST(reader_signals, lenon_inhibits_feed)
{
    struct ge g;
    ge_init(&g);
    ge_log_set_active_types(0);
    ge_clear(&g);

    /* Normal (lenon=0): CE09 raises the feed line. */
    g.integrated_reader.lenon = 0;
    reader_send_tu10(&g);
    ASSERT_EQ((int)g.integrated_reader.tu03, 1);

    /* Manual (lenon=1): the feed strobe is suppressed. */
    g.integrated_reader.tu03 = 0;
    g.integrated_reader.lenon = 1;
    reader_send_tu10(&g);
    ASSERT_EQ((int)g.integrated_reader.tu03, 0);
}
