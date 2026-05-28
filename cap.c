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
