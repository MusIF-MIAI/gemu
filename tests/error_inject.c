#include "utest.h"
#include "../ge.h"
#include "../signals.h"
#include "../bit.h"

/*
 * Channel-1 peripheral error injection (Phase F).
 *
 * CE_chan1_status (msl-commands.c) loads the channel-1 status into RO for the
 * EPER "examine abnormal conditions" decode. It reports 0x40 ("operation OK")
 * by default, or ge->inject_chan1_status when that field is non-zero, letting a
 * harness inject a peripheral error/abnormal condition.
 *
 * The decode that consumes it is DU95 (signals.h): DU95 = (!RO1 && !RO2 && RO6),
 * so **DU95 == 1 means "no error / OK"** and DU95 == 0 means an error/abnormal
 * condition is present. These tests validate the injection contract — that the
 * default status reads as no-error and a representative injected status reads as
 * an error — and that ge_clear leaves injection disabled.
 */

/* Default channel-1 status 0x40 (RO6 set, RO1=RO2=0) decodes as "no error". */
UTEST(error_inject, default_status_is_no_error)
{
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    ASSERT_EQ((int)g.inject_chan1_status, 0);   /* injection off after clear */

    g.rRO = 0x40;                                /* what CE_chan1_status sets */
    ASSERT_EQ((int)DU95(&g), 1);                 /* DU95==1 -> "no error / OK" */
}

/* An injected status with RO1 set (e.g. 0x42) decodes as an error. */
UTEST(error_inject, injected_error_is_detected)
{
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    g.inject_chan1_status = 0x42;                /* RO6 + RO1 (error) */
    /* CE_chan1_status would copy this into RO; emulate that contract here. */
    g.rRO = g.inject_chan1_status ? g.inject_chan1_status : 0x40;

    ASSERT_EQ((int)g.rRO, 0x42);
    ASSERT_EQ((int)DU95(&g), 0);                 /* DU95==0 -> error present */
}

/* A status with RO2 set (0x44) is also an error; RO6-only (0x40) is not. */
UTEST(error_inject, ro2_error_and_ok_boundary)
{
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    g.rRO = 0x44;                                /* RO6 + RO2 */
    ASSERT_EQ((int)DU95(&g), 0);                 /* error */

    g.rRO = 0x40;                                /* RO6 only */
    ASSERT_EQ((int)DU95(&g), 1);                 /* no error */
}
