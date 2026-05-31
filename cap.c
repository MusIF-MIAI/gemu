/*
 * cap.c — parser for Raspberry-Pi-Pico punch-card reader .cap files.
 *
 * File format:
 *   - Free-text preamble lines, bare column counters, "FEED ON/OFF" etc.
 *   - Card delimiter: a line of the exact form "Card n. N" (N = 1, 2, 3, …)
 *   - Following the delimiter: whitespace/newline-separated 4-hex-digit tokens,
 *     one per card column (80 per card), each representing a 12-bit hole pattern.
 *   - Non-hex lines between card data are ignored.
 *
 * This parser is fully reentrant; no global state.
 */

#include "cap.h"
#include "transcode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* -------------------------------------------------------------------------
 * Internal data structures
 * ------------------------------------------------------------------------- */

struct cap_card {
    uint16_t *cols;   /* dynamically allocated column data */
    int       ncols;  /* number of columns stored */
    int       cap;    /* allocated capacity */
};

struct cap_deck {
    struct cap_card *cards;  /* dynamically allocated card array */
    int              ncards; /* number of cards stored */
    int              cap;    /* allocated capacity */
};

/* -------------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------------- */

/* Grow the card array in the deck by one slot. Returns 0 on success. */
static int deck_add_card(struct cap_deck *d)
{
    if (d->ncards >= d->cap) {
        int newcap = d->cap ? d->cap * 2 : 16;
        struct cap_card *tmp = realloc(d->cards,
                                       (size_t)newcap * sizeof(struct cap_card));
        if (!tmp)
            return -1;
        d->cards = tmp;
        d->cap   = newcap;
    }
    d->cards[d->ncards].cols  = NULL;
    d->cards[d->ncards].ncols = 0;
    d->cards[d->ncards].cap   = 0;
    d->ncards++;
    return 0;
}

/* Append one column value to the current (last) card. Returns 0 on success. */
static int card_add_col(struct cap_card *c, uint16_t val)
{
    if (c->ncols >= c->cap) {
        int newcap = c->cap ? c->cap * 2 : 80;
        uint16_t *tmp = realloc(c->cols, (size_t)newcap * sizeof(uint16_t));
        if (!tmp)
            return -1;
        c->cols = tmp;
        c->cap  = newcap;
    }
    c->cols[c->ncols++] = val & 0x1FFFu; /* keep 13 bits (12-bit hole + spare) */
    return 0;
}

/*
 * Return non-zero if `s` is a valid 4-hex-digit token (exactly 4 chars,
 * all hexadecimal digits).
 */
static int is_hex4(const char *s)
{
    if (!s || strlen(s) != 4)
        return 0;
    return isxdigit((unsigned char)s[0]) &&
           isxdigit((unsigned char)s[1]) &&
           isxdigit((unsigned char)s[2]) &&
           isxdigit((unsigned char)s[3]);
}

/*
 * Return non-zero if line begins with "Card n. " (case-sensitive).
 * If it does, *card_num receives the card number (>= 1).
 */
static int parse_card_header(const char *line, int *card_num)
{
    const char prefix[] = "Card n. ";
    const size_t plen   = sizeof(prefix) - 1;
    if (strncmp(line, prefix, plen) != 0)
        return 0;
    const char *p = line + plen;
    /* Require at least one digit immediately after the prefix */
    if (!isdigit((unsigned char)*p))
        return 0;
    char *end;
    long n = strtol(p, &end, 10);
    if (n < 1)
        return 0;
    /* Allow trailing whitespace/newline only */
    while (*end && (isspace((unsigned char)*end) || *end == '\r'))
        end++;
    if (*end != '\0')
        return 0;
    *card_num = (int)n;
    return 1;
}

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

struct cap_deck *cap_load(const char *path)
{
    if (!path)
        return NULL;

    FILE *fp = fopen(path, "r");
    if (!fp)
        return NULL;

    struct cap_deck *d = calloc(1, sizeof(struct cap_deck));
    if (!d) {
        fclose(fp);
        return NULL;
    }

    int in_card = 0;  /* whether we are inside a card's data section */

    char line[4096];
    while (fgets(line, sizeof(line), fp)) {
        /* Strip trailing newline/CR */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';

        /* Check for card header */
        int card_num = 0;
        if (parse_card_header(line, &card_num)) {
            if (deck_add_card(d) != 0)
                goto oom;
            in_card = 1;
            continue;
        }

        /* If we are not yet inside any card, skip non-card lines */
        if (!in_card)
            continue;

        /*
         * We are inside a card data section.  Tokenize the line and
         * collect 4-hex-digit tokens; ignore anything else (counters,
         * "FEED ON/OFF", blank lines, etc.).
         */
        char *buf = line;
        char *tok;
        /* Use a copy because strtok modifies its argument */
        char linecopy[4096];
        strncpy(linecopy, line, sizeof(linecopy) - 1);
        linecopy[sizeof(linecopy) - 1] = '\0';

        tok = strtok(linecopy, " \t\r\n");
        while (tok) {
            if (is_hex4(tok)) {
                uint16_t val = (uint16_t)strtoul(tok, NULL, 16);
                struct cap_card *cur = &d->cards[d->ncards - 1];
                if (card_add_col(cur, val) != 0)
                    goto oom;
            }
            tok = strtok(NULL, " \t\r\n");
        }
        (void)buf; /* suppress unused-variable warning */
    }

    fclose(fp);
    return d;

oom:
    fclose(fp);
    cap_free(d);
    return NULL;
}

int cap_num_cards(const struct cap_deck *d)
{
    if (!d)
        return 0;
    return d->ncards;
}

int cap_card_ncols(const struct cap_deck *d, int i)
{
    if (!d || i < 0 || i >= d->ncards)
        return 0;
    return d->cards[i].ncols;
}

const uint16_t *cap_card_columns(const struct cap_deck *d, int i)
{
    if (!d || i < 0 || i >= d->ncards)
        return NULL;
    return d->cards[i].cols;
}

void cap_free(struct cap_deck *d)
{
    if (!d)
        return;
    for (int i = 0; i < d->ncards; i++)
        free(d->cards[i].cols);
    free(d->cards);
    free(d);
}

/* -------------------------------------------------------------------------
 * Self-addressed scatter load (mirrors gdis --image; see cap.h).
 * ------------------------------------------------------------------------- */

static int scat_decode_card(const uint16_t *cols, int ncols, int mode,
                            uint8_t out[80])
{
    int n = ncols < 80 ? ncols : 80;

    for (int i = 0; i < n; i++)
        out[i] = transcode_column(cols[i], (enum transcode_mode)mode);

    return n;
}

/* Detect the deck's dominant 8-byte data-card prefix (cols 0-7). Returns the
 * match count and copies it into out[8]; 0 if no eligible cards. */
static int scat_detect_prefix(struct cap_deck *d, int mode, uint8_t out[8])
{
    int nc = cap_num_cards(d);
    struct { uint8_t p[8]; int cnt; } tab[1024];
    int ntab = 0, best = -1;

    for (int i = 0; i < nc; i++) {
        int ncols = cap_card_ncols(d, i);
        const uint16_t *cols = cap_card_columns(d, i);
        if (ncols < 11 || !cols)
            continue;

        uint8_t b[80];
        scat_decode_card(cols, ncols, mode, b);

        int j;
        for (j = 0; j < ntab; j++) {
            if (memcmp(tab[j].p, b, 8) == 0) {
                tab[j].cnt++;
                break;
            }
        }
        if (j == ntab && ntab < 1024) {
            memcpy(tab[ntab].p, b, 8);
            tab[ntab].cnt = 1;
            ntab++;
        }
    }

    for (int j = 0; j < ntab; j++) {
        if (best < 0 || tab[j].cnt > tab[best].cnt)
            best = j;
    }
    if (best < 0)
        return 0;

    memcpy(out, tab[best].p, 8);
    return tab[best].cnt;
}

int cap_load_scattered(const char *path, int mode, unsigned char *image,
                       unsigned *lo, unsigned *hi)
{
    struct cap_deck *d = cap_load(path);
    if (!d)
        return -1;

    uint8_t want[8];
    int have_prefix = (scat_detect_prefix(d, mode, want) >= 4);
    int loose = !have_prefix;  /* no dominant prefix: accept any fitting record */

    int nc = cap_num_cards(d);
    int loaded = 0;
    long min_a = -1, max_a = -1;

    for (int i = 0; i < nc; i++) {
        int ncols = cap_card_ncols(d, i);
        const uint16_t *cols = cap_card_columns(d, i);
        if (ncols < 11 || !cols)
            continue;

        uint8_t b[80];
        int n = scat_decode_card(cols, ncols, mode, b);

        int match = (have_prefix && n >= 8 && memcmp(b, want, 8) == 0);
        int ll    = b[8];
        long addr = ((long)b[9] << 8) | b[10];
        int paylen = ll + 1;
        int fits = (11 + ll < n) && (addr + ll <= 0xFFFF);

        if (!(loose ? fits : (match && fits)))
            continue;

        for (int k = 0; k < paylen; k++)
            image[addr + k] = b[11 + k];

        if (min_a < 0 || addr < min_a)
            min_a = addr;
        if (addr + ll > max_a)
            max_a = addr + ll;
        loaded++;
    }

    cap_free(d);

    if (loaded == 0 || min_a < 0)
        return -1;

    if (lo)
        *lo = (unsigned)min_a;
    if (hi)
        *hi = (unsigned)max_a;
    return loaded;
}
