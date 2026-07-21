#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utest.h"
#include "decks.h"
#include "../cap.h"
#include "../binimage.h"
#include "../transcode.h"

static uint16_t test_colbin_encode_byte(uint8_t b)
{
    uint16_t col = 0;

    if (b & 0x80) col |= (uint16_t)(1u << 0);
    if (b & 0x40) col |= (uint16_t)(1u << 1);
    if (b & 0x20) col |= (uint16_t)(1u << 2);
    if (b & 0x10) col |= (uint16_t)(1u << 3);
    if (b & 0x08) col |= (uint16_t)(1u << 6);
    if (b & 0x04) col |= (uint16_t)(1u << 7);
    if (b & 0x02) col |= (uint16_t)(1u << 8);
    if (b & 0x01) col |= (uint16_t)(1u << 9);

    return col;
}

static int write_repeated_card_cap(const char *path, const uint16_t cols[80], int count)
{
    FILE *f = fopen(path, "w");
    if (!f)
        return -1;
    fprintf(f, "Synthetic cap for capcat test\n");
    for (int card = 0; card < count; card++) {
        fprintf(f, "Card n. %d\n", card + 1);
        for (int i = 0; i < 80; i++)
            fprintf(f, "%04X%c", cols[i] & 0x1FFFu, (i == 79) ? '\n' : ' ');
    }
    fclose(f);
    return 0;
}

static int file_exists(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return 0;
    fclose(f);
    return 1;
}

UTEST(cap, parse_funktional)
{
    /* The DUMP1 decks live outside this git repo, so they are absent in a CI
     * checkout. Skip gracefully when the deck is not present (mirrors the skip
     * pattern in tests/bootstrap.c); run the real assertions otherwise. */
    const char *path = deck_funktionalcpu_cap();
    FILE *probe = fopen(path, "r");
    if (!probe) {
        printf("  [SKIP] %s not found\n", path);
        return;
    }
    fclose(probe);

    struct cap_deck *d = cap_load(path);
    ASSERT_TRUE(d != NULL);
    ASSERT_EQ(cap_num_cards(d), 228);
    ASSERT_EQ(cap_card_ncols(d, 0), 80);
    ASSERT_EQ((int)cap_card_columns(d, 0)[0], 0x0010);
    cap_free(d);
}

UTEST(cap, missing_file)
{
    struct cap_deck *d = cap_load("DUMP1/does-not-exist.cap");
    ASSERT_TRUE(d == NULL);
}

UTEST(cap, classify_real_deck_families)
{
    struct {
        const char *path;
        enum cap_deck_family family;
    } cases[] = {
        { "../DUMP1/funktionalcpu.cap", CAP_FAMILY_SCATTER },
        { "../DUMP1/printermechanicaltest.cap", CAP_FAMILY_SCATTER },
        { "../DUMP1/control-program-cr.cap", CAP_FAMILY_SCATTER },
        { "../DUMP1/isolationcpu01.cap", CAP_FAMILY_ISOLATION },
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        struct cap_deck *d;
        if (!file_exists(cases[i].path)) {
            printf("  [SKIP] %s not found\n", cases[i].path);
            continue;
        }
        d = cap_load(cases[i].path);
        ASSERT_TRUE(d != NULL);
        ASSERT_EQ((int)cap_detect_family(d, TC_COLBIN, NULL), (int)cases[i].family);
        cap_free(d);
    }
}

UTEST(cap, scattered_real_decks_land_at_expected_addresses)
{
    struct {
        const char *path;
        unsigned lo;
        unsigned hi;
        unsigned addr;
        uint8_t bytes[4];
    } cases[] = {
        { "../DUMP1/funktionalcpu.cap",       0x0000u, 0x1C53u, 0x0100u, { 0x43, 0xF0, 0x17, 0x2A } },
        { "../DUMP1/printermechanicaltest.cap", 0x001Eu, 0x0BF1u, 0x001Eu, { 0x01, 0x18, 0x00, 0x00 } },
        { "../DUMP1/control-program-cr.cap", 0x0004u, 0x1035u, 0x0004u, { 0x43, 0xF0, 0x02, 0x7A } },
    };
    static uint8_t image[65536];

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        unsigned lo = 0, hi = 0;
        int nc;
        if (!file_exists(cases[i].path)) {
            printf("  [SKIP] %s not found\n", cases[i].path);
            continue;
        }
        memset(image, 0, sizeof(image));
        nc = cap_load_scattered(cases[i].path, TC_COLBIN, image, &lo, &hi);
        ASSERT_GT(nc, 0);
        ASSERT_EQ(lo, cases[i].lo);
        ASSERT_EQ(hi, cases[i].hi);
        ASSERT_EQ(image[cases[i].addr + 0], cases[i].bytes[0]);
        ASSERT_EQ(image[cases[i].addr + 1], cases[i].bytes[1]);
        ASSERT_EQ(image[cases[i].addr + 2], cases[i].bytes[2]);
        ASSERT_EQ(image[cases[i].addr + 3], cases[i].bytes[3]);
    }
}

UTEST(cap, isolation_stream_real_deck)
{
    static const char path[] = "../DUMP1/isolationcpu01.cap";
    static uint8_t image[65536];
    unsigned lo = 0, hi = 0;
    int nc;

    if (!file_exists(path)) {
        printf("  [SKIP] %s not found\n", path);
        return;
    }

    memset(image, 0, sizeof(image));
    nc = cap_load_isolation_stream(path, image, 0x0000u, &lo, &hi);
    ASSERT_EQ(nc, 210);
    ASSERT_EQ(lo, 0x0000u);
    ASSERT_EQ(hi, 0x3E57u);
    ASSERT_EQ(image[0x0000], 0x9E);
    ASSERT_EQ(image[0x0001], 0x80);
    ASSERT_EQ(image[0x0002], 0x00);
    ASSERT_EQ(image[0x0003], 0x04);
}

UTEST(cap, capcat_appends_overlay_cards)
{
    static const char base_path[] = "/tmp/gemu_capcat_base.cap";
    static const char overlay_path[] = "/tmp/gemu_capcat_overlay.bin";
    static const char out_path[] = "/tmp/gemu_capcat_out.cap";
    static const char tool_path[] = "tools/capcat";
    static const uint8_t prefix[8] = { 0x00, 0x04, 0x40, 0x00, 0x20, 0x40, 0x40, 0x42 };
    uint16_t cols[80] = {0};
    uint8_t image[65536];
    unsigned lo = 0, hi = 0;
    char cmd[512];
    FILE *probe;

    probe = fopen(tool_path, "r");
    if (!probe) {
        printf("  [SKIP] %s not built\n", tool_path);
        return;
    }
    fclose(probe);

    for (int i = 0; i < 8; i++)
        cols[i] = test_colbin_encode_byte(prefix[i]);
    cols[8] = test_colbin_encode_byte(0x01);  /* 2 payload bytes */
    cols[9] = test_colbin_encode_byte(0x01);
    cols[10] = test_colbin_encode_byte(0x00);
    cols[11] = test_colbin_encode_byte(0xAA);
    cols[12] = test_colbin_encode_byte(0x55);
    ASSERT_EQ(write_repeated_card_cap(base_path, cols, 4), 0);

    {
        uint8_t ov[3] = { 0x11, 0x22, 0x33 };
        FILE *f = fopen(overlay_path, "wb");
        ASSERT_TRUE(f != NULL);
        ASSERT_EQ(binimage_write(f, 0x0200, 0x0200, ov, (uint16_t)sizeof(ov)), BINIMAGE_OK);
        fclose(f);
    }

    snprintf(cmd, sizeof(cmd),
             "%s --prefix \"00 04 40 00 20 40 40 42\" -o %s %s %s "
             ">/tmp/gemu_capcat_cmd.log 2>&1",
             tool_path, out_path, base_path, overlay_path);
    ASSERT_EQ(system(cmd), 0);

    memset(image, 0, sizeof(image));
    ASSERT_GT(cap_load_scattered(out_path, TC_COLBIN, image, &lo, &hi), 0);
    ASSERT_EQ(lo, 0x0100u);
    ASSERT_EQ(image[0x0100], 0xAA);
    ASSERT_EQ(image[0x0101], 0x55);
    ASSERT_EQ(image[0x0200], 0x11);
    ASSERT_EQ(image[0x0201], 0x22);
    ASSERT_EQ(image[0x0202], 0x33);
}

UTEST(cap, capcat_rejects_isolation_base)
{
    static const char base_path[] = "../DUMP1/isolationcpu01.cap";
    static const char overlay_path[] = "/tmp/gemu_capcat_iso_overlay.bin";
    static const char out_path[] = "/tmp/gemu_capcat_iso_out.cap";
    static const char tool_path[] = "tools/capcat";
    char cmd[512];
    FILE *probe;

    if (!file_exists(base_path)) {
        printf("  [SKIP] %s not found\n", base_path);
        return;
    }

    probe = fopen(tool_path, "r");
    if (!probe) {
        printf("  [SKIP] %s not built\n", tool_path);
        return;
    }
    fclose(probe);

    {
        uint8_t ov[1] = { 0x11 };
        FILE *f = fopen(overlay_path, "wb");
        ASSERT_TRUE(f != NULL);
        ASSERT_EQ(binimage_write(f, 0x0200, 0x0200, ov, (uint16_t)sizeof(ov)), BINIMAGE_OK);
        fclose(f);
    }

    snprintf(cmd, sizeof(cmd), "%s -o %s %s %s >/tmp/gemu_capcat_iso_cmd.log 2>&1",
             tool_path, out_path, base_path, overlay_path);
    ASSERT_NE(system(cmd), 0);
}
