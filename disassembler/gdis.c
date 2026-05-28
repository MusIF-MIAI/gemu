/*
 * gdis — GE-120 / GE-130 disassembler & "depuncher".
 *
 * Two jobs, in one pipeline:
 *
 *   1. DEPUNCH:  read a .cap punch-card capture, column-binary ("by-pass")
 *      decode each program card (reusing the emulator's own cap.c parser and
 *      transcode.c COLBIN decoder), then interpret the self-describing
 *      data-card records (LL / load-address / payload) into a flat memory
 *      image.  This is the inverse of punching a deck.
 *
 *   2. DISASSEMBLE:  linear-sweep the reconstructed image (or a raw .bin) into
 *      readable GE-120 assembly — the inverse of the `gasm` assembler.  The
 *      output re-assembles with gasm (ORG directives, labels on branch
 *      targets, absolute address fields).
 *
 * Sources of truth (all in the gemu tree):
 *   cap.c / cap.h         — .cap parser (cards -> 80 x 13-bit columns).
 *   transcode.c           — transcode_column(): the COLBIN B2R decode.
 *   docs/punchcards.md §5 — data-card layout: col 8 = LL, cols 9-10 = load
 *                           address (big-endian), cols 11..(11+LL) = LL+1
 *                           payload bytes; data cards carry a constant 8-byte
 *                           ID prefix in cols 0-7.
 *   opcodes.h, msl-commands.c, signals.h — opcode/format/aux encoding (the
 *                           same constants gasm.c encodes; see ISA.md App. A).
 *
 * Builds against ../cap.c and ../transcode.c (see Makefile).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "cap.h"
#include "transcode.h"
#include "binimage.h"

/* ------------------------------------------------------------------ */
/* Reverse opcode table (mirror of gasm.c / opcodes.h — keep in sync)  */
/* ------------------------------------------------------------------ */

typedef enum {
    D_P,        /* P, 2 bytes: bare mnemonic (HLT/NOP2)                 */
    D_P02,      /* P, 2 bytes: 0x02 sub-function selected by 2nd char   */
    D_BRANCH,   /* PM: JC/JCC   -> "mn mask, addr"                      */
    D_JU,       /* PM: JU       -> "mn addr"                            */
    D_SENSE,    /* PM: 0x53     -> JS1/JS2/JIE by aux, "mn addr"        */
    D_REG,      /* PM: LR/...   -> "mn N, addr"                         */
    D_IMM,      /* PM: MVI/...  -> "mn K, addr"                         */
    D_PER,      /* PM: PER/...  -> "mn aux, addr"                       */
    D_SS1,      /* SS, 6 bytes: "mn len, A1, A2"                        */
    D_SS2       /* SS, 6 bytes: "mn l1, l2, A1, A2"                     */
} dfmt_t;

struct dmnem { uint8_t op; const char *name; dfmt_t fmt; int len; };

/* PM and SS opcodes keyed by the opcode byte. P-format and the 0x53 sense
 * group are handled specially (by 2nd char / aux) in decode_at(). */
static const struct dmnem DTAB[] = {
    /* PM branches / jumps */
    { 0x43, "JC",   D_BRANCH, 4 },
    { 0x40, "JCC",  D_BRANCH, 4 },
    { 0x47, "JU",   D_JU,     4 },
    { 0x41, "JRT",  D_PER,    4 },   /* reserved (no decode path)        */
    /* PM register / address */
    { 0xBC, "LR",   D_REG, 4 },
    { 0x84, "STR",  D_REG, 4 },
    { 0x68, "LA",   D_REG, 4 },
    { 0xBD, "CMR",  D_REG, 4 },
    { 0xBE, "AMR",  D_REG, 4 },
    { 0xBF, "SMR",  D_REG, 4 },
    /* PM immediate */
    { 0x92, "MVI",  D_IMM, 4 },
    { 0x94, "NI",   D_IMM, 4 },
    { 0x95, "CMI",  D_IMM, 4 },
    { 0x96, "CI",   D_IMM, 4 },
    { 0x97, "XI",   D_IMM, 4 },
    { 0x91, "TM",   D_IMM, 4 },
    /* PM peripheral / misc */
    { 0x9E, "PER",  D_PER, 4 },
    { 0x9C, "PERI", D_PER, 4 },
    { 0x90, "RDC",  D_PER, 4 },
    { 0x9D, "LPSR", D_PER, 4 },   /* reserved (no decode path)            */
    /* SS single-length */
    { 0xD2, "MVC",  D_SS1, 6 },
    { 0xD4, "NC",   D_SS1, 6 },
    { 0xD5, "CMC",  D_SS1, 6 },
    { 0xD6, "OC",   D_SS1, 6 },
    { 0xD7, "XC",   D_SS1, 6 },
    { 0xDC, "TL",   D_SS1, 6 },
    { 0xF8, "MVQ",  D_SS1, 6 },
    { 0xF9, "CMQ",  D_SS1, 6 },
    { 0xDE, "EDT",  D_SS1, 6 },
    { 0xD9, "SR",   D_SS1, 6 },   /* SS; length encoding unconfirmed      */
    { 0xDB, "SL",   D_SS1, 6 },   /* SS; length encoding unconfirmed      */
    /* SS two-length */
    { 0xDA, "PK",   D_SS2, 6 },
    { 0xD8, "UPK",  D_SS2, 6 },
    { 0xEE, "PKS",  D_SS2, 6 },
    { 0xEF, "UPKS", D_SS2, 6 },
    { 0xE8, "MVP",  D_SS2, 6 },
    { 0xE9, "CMP",  D_SS2, 6 },
    { 0xEA, "AP",   D_SS2, 6 },
    { 0xEB, "SP",   D_SS2, 6 },
    { 0xEC, "MP",   D_SS2, 6 },
    { 0xED, "DP",   D_SS2, 6 },
    { 0xFA, "AD",   D_SS2, 6 },
    { 0xFB, "SD",   D_SS2, 6 },
    { 0xFE, "AB",   D_SS2, 6 },
    { 0xFF, "SB",   D_SS2, 6 },
};
#define NDTAB ((int)(sizeof(DTAB)/sizeof(DTAB[0])))

static const struct dmnem *dlookup(uint8_t op)
{
    for (int i = 0; i < NDTAB; i++)
        if (DTAB[i].op == op) return &DTAB[i];
    return NULL;
}

/* Best-effort alias name for a 4-bit branch condition mask (high nibble). */
static const char *mask_alias(uint8_t mask)
{
    switch (mask & 0xF0) {
        case 0xF0: return "JMP/JANY";
        case 0x40: return "JL/JLT";
        case 0x20: return "JE/JEQ/JZ";
        case 0x10: return "JH/JGT";
        case 0x50: return "JNE/JNZ";
        case 0x60: return "JLE";
        case 0x30: return "JGE";
        case 0x80: return "JOV";
        default:   return NULL;
    }
}

/* ------------------------------------------------------------------ */
/* Memory image (sparse: present[] marks loaded bytes)                 */
/* ------------------------------------------------------------------ */

static uint8_t image[65536];
static uint8_t present[65536];
static long img_min = -1, img_max = 0;

static void put(long addr, uint8_t b)
{
    if (addr < 0 || addr > 0xFFFF) return;
    image[addr] = b; present[addr] = 1;
    if (img_min < 0 || addr < img_min) img_min = addr;
    if (addr + 1 > img_max) img_max = addr + 1;
}

/* labels[] marks addresses that are branch targets (so we print L_xxxx:).
 * Only targets that land INSIDE the loaded image are labelled — otherwise the
 * operand would reference a symbol with no definition line and the output would
 * not re-assemble. Out-of-image targets are printed as absolute hex instead. */
static uint8_t labels[65536];

static void mark_label(uint16_t field)
{
    if (img_min >= 0 && field >= img_min && field < img_max)
        labels[field] = 1;
}

/* ------------------------------------------------------------------ */
/* .cap depunching                                                     */
/* ------------------------------------------------------------------ */

/* funktionalcpu's data-card prefix is 00 04 40 00 20 40 40 42 (punchcards.md
 * §5). That value turned out to be PER-DECK — constant within a deck but
 * different between decks — so gdis auto-detects each deck's prefix rather than
 * hardcoding this one. Decks in the same family start 00 04 .. .. .. 40 .. .. */

/* Decode one card's columns to GE bytes via the chosen mode. Returns count. */
static int decode_card(const uint16_t *cols, int ncols, enum transcode_mode m,
                       uint8_t out[80])
{
    int n = ncols < 80 ? ncols : 80;
    for (int i = 0; i < n; i++) out[i] = transcode_column(cols[i], m);
    return n;
}

/*
 * Find the deck's dominant 8-byte data-card prefix (cols 0-7 after COLBIN).
 * punchcards.md §5 found the prefix is constant across a deck's data cards;
 * since it is PER-DECK we learn it here. Returns the match count of the most
 * common prefix and copies it into out[8]; 0 if there are no eligible cards.
 */
static int detect_prefix(struct cap_deck *d, enum transcode_mode mode, uint8_t out[8])
{
    int nc = cap_num_cards(d);
    struct { uint8_t p[8]; int cnt; } tab[1024];
    int ntab = 0, best = -1;

    for (int i = 0; i < nc; i++) {
        int ncols = cap_card_ncols(d, i);
        const uint16_t *cols = cap_card_columns(d, i);
        if (ncols < 11 || !cols) continue;
        uint8_t b[80];
        decode_card(cols, ncols, mode, b);
        int j;
        for (j = 0; j < ntab; j++)
            if (memcmp(tab[j].p, b, 8) == 0) { tab[j].cnt++; break; }
        if (j == ntab && ntab < 1024) { memcpy(tab[ntab].p, b, 8); tab[ntab].cnt = 1; ntab++; }
    }
    for (int j = 0; j < ntab; j++)
        if (best < 0 || tab[j].cnt > tab[best].cnt) best = j;
    if (best < 0) return 0;
    memcpy(out, tab[best].p, 8);
    return tab[best].cnt;
}

/*
 * Load a .cap deck into the image. Each hex card (>= 11 cols) is COLBIN-decoded
 * and interpreted as a self-describing record: b[8]=LL, b[9..10]=load address,
 * b[11..11+LL]=LL+1 payload bytes.
 *
 * Card acceptance:
 *   - loose mode:        any self-consistent record (length + address fit).
 *   - user_prefix != 0:  only cards whose cols 0-7 equal that prefix.
 *   - otherwise (auto):  the deck's dominant prefix is detected and used.
 * Returns 0 on success.
 */
static int load_cap(const char *path, enum transcode_mode mode, int loose,
                    int verbose, const uint8_t *user_prefix)
{
    struct cap_deck *d = cap_load(path);
    if (!d) { fprintf(stderr, "gdis: cannot parse .cap '%s'\n", path); return -1; }

    int nc = cap_num_cards(d);
    int loaded = 0, skipped = 0;

    /* Decide which prefix gates payload cards (unless --loose). */
    uint8_t want[8];
    int have_prefix = 0;
    if (!loose) {
        if (user_prefix) { memcpy(want, user_prefix, 8); have_prefix = 1; }
        else {
            uint8_t det[8];
            int cnt = detect_prefix(d, mode, det);
            if (cnt >= 4) { memcpy(want, det, 8); have_prefix = 1;
                fprintf(stderr, "gdis: auto-detected data-card prefix"
                        " %02X %02X %02X %02X %02X %02X %02X %02X (%d cards)\n",
                        want[0],want[1],want[2],want[3],want[4],want[5],want[6],want[7], cnt);
                if (want[0] != 0x00 || want[1] != 0x04)
                    fprintf(stderr, "gdis: note: prefix does not start 00 04 — this deck likely "
                            "uses a different card framing than the funktionalcpu family; "
                            "inspect with --cards, or try --loose.\n");
            } else {
                fprintf(stderr, "gdis: no dominant data-card prefix found — deck may use a "
                        "different framing; loading nothing. Try --loose or --cards.\n");
            }
        }
    }

    for (int i = 0; i < nc; i++) {
        int ncols = cap_card_ncols(d, i);
        const uint16_t *cols = cap_card_columns(d, i);
        if (ncols < 11 || !cols) { skipped++; continue; }  /* art/short cards */

        uint8_t b[80];
        int n = decode_card(cols, ncols, mode, b);

        int match = (have_prefix && n >= 8 && memcmp(b, want, 8) == 0);
        int ll   = b[8];
        long addr = ((long)b[9] << 8) | b[10];
        int paylen = ll + 1;
        int fits = (11 + ll < n) && (addr + ll <= 0xFFFF);

        int accept = loose ? fits : (match && fits);
        if (!accept) {
            if (verbose)
                fprintf(stderr, "  card %3d: skip (cols=%d match=%d LL=0x%02X "
                        "addr=0x%04lX)\n", i, ncols, match, ll, addr);
            skipped++;
            continue;
        }
        for (int k = 0; k < paylen; k++)
            put(addr + k, b[11 + k]);
        if (verbose)
            fprintf(stderr, "  card %3d: load 0x%04lX..0x%04lX (%d bytes)\n",
                    i, addr, addr + paylen - 1, paylen);
        loaded++;
    }

    fprintf(stderr, "gdis: %s: %d cards, %d records loaded, %d skipped; "
            "image 0x%04lX..0x%04lX (%ld bytes)\n",
            path, nc, loaded, skipped,
            img_min < 0 ? 0 : img_min, img_max ? img_max - 1 : 0,
            img_min < 0 ? 0 : img_max - img_min);
    cap_free(d);
    return loaded ? 0 : -1;
}

/* ------------------------------------------------------------------ */
/* Disassembly                                                         */
/* ------------------------------------------------------------------ */

static int all_present(long a, int len)
{
    if (a + len > img_max) return 0;
    for (int i = 0; i < len; i++) if (!present[a + i]) return 0;
    return 1;
}

/* Format a 16-bit address field as an operand. If a label exists at the field
 * value, use it; otherwise the absolute hex value (round-trips through gasm). */
static void fmt_addr(uint16_t field, char *buf, size_t n)
{
    if (labels[field]) snprintf(buf, n, "L_%04X", field);
    else               snprintf(buf, n, "0x%04X", field);
}

/* Decode the instruction at addr. On pass 1 record branch-target labels; on
 * pass 2 write the assembly line to out. Returns the instruction length. */
static int decode_at(long addr, int pass, FILE *out)
{
    uint8_t op = image[addr];
    char a1[32], a2[32];

    /* ---- P format (top two bits 00) ---- */
    if (op < 0x40) {
        if (!all_present(addr, 2)) goto db1;
        uint8_t c2 = image[addr + 1];
        const char *mn = NULL;
        if (op == 0x0A) mn = "HLT";
        else if (op == 0x07) mn = "NOP2";
        else if (op == 0x02) {
            switch (c2) {
                case 0x10: mn = "ENS";  break;
                case 0x20: mn = "INS";  break;
                case 0x40: mn = "LOFF"; break;
                case 0x80: mn = "LON";  break;
                case 0x91: mn = "LOLL"; break;
            }
        }
        if (!mn) goto db1;
        if (pass == 2) fprintf(out, "        %-6s\n", mn);
        return 2;
    }

    /* ---- PM format (4 bytes) ---- */
    if (op < 0xC0) {
        if (!all_present(addr, 4)) goto db1;
        uint8_t aux = image[addr + 1];
        uint16_t field = (uint16_t)((image[addr + 2] << 8) | image[addr + 3]);

        /* 0x53 sense group: select by aux (not in DTAB — handled here) */
        if (op == 0x53) {
            const char *mn = (aux == 0x80) ? "JS1"
                           : (aux == 0x40) ? "JS2"
                           : (aux == 0x20) ? "JIE" : NULL;
            if (!mn) goto db1;
            if (pass == 1) mark_label(field);
            else { fmt_addr(field, a1, sizeof a1); fprintf(out, "        %-6s %s\n", mn, a1); }
            return 4;
        }

        const struct dmnem *m = dlookup(op);
        if (!m) goto db1;
        switch (m->fmt) {
        case D_JU:
            if (pass == 1) mark_label(field);
            else { fmt_addr(field, a1, sizeof a1); fprintf(out, "        %-6s %s\n", m->name, a1); }
            return 4;
        case D_BRANCH:
            if (pass == 1) mark_label(field);
            else {
                fmt_addr(field, a1, sizeof a1);
                const char *al = mask_alias(aux);
                if (al) fprintf(out, "        %-6s 0x%02X, %s   ; %s\n", m->name, aux & 0xF0, a1, al);
                else    fprintf(out, "        %-6s 0x%02X, %s\n", m->name, aux & 0xF0, a1);
            }
            return 4;
        case D_REG:
            if (pass == 2) { fmt_addr(field, a1, sizeof a1);
                fprintf(out, "        %-6s %d, %s\n", m->name, aux & 7, a1); }
            return 4;
        case D_IMM:
            if (pass == 2) { fmt_addr(field, a1, sizeof a1);
                fprintf(out, "        %-6s 0x%02X, %s\n", m->name, aux, a1); }
            return 4;
        case D_PER:
            if (pass == 2) { fmt_addr(field, a1, sizeof a1);
                fprintf(out, "        %-6s 0x%02X, %s\n", m->name, aux, a1); }
            return 4;
        default: goto db1;
        }
    }

    /* ---- SS format (6 bytes) ---- */
    {
        const struct dmnem *m = dlookup(op);
        if (!m) goto db1;
        if (!all_present(addr, 6)) goto db1;
        uint8_t ll = image[addr + 1];
        uint16_t A1 = (uint16_t)((image[addr + 2] << 8) | image[addr + 3]);
        uint16_t A2 = (uint16_t)((image[addr + 4] << 8) | image[addr + 5]);
        if (pass == 2) {
            fmt_addr(A1, a1, sizeof a1);
            fmt_addr(A2, a2, sizeof a2);
            if (m->fmt == D_SS1)
                fprintf(out, "        %-6s %d, %s, %s\n", m->name, ll + 1, a1, a2);
            else
                fprintf(out, "        %-6s %d, %d, %s, %s\n", m->name,
                        ((ll >> 4) & 0xf) + 1, (ll & 0xf) + 1, a1, a2);
        }
        return 6;
    }

db1:
    /* Unknown/garbled or truncated: emit one data byte and resync. */
    if (pass == 2) fprintf(out, "        DB     0x%02X        ; %c\n",
                           op, (op >= 0x20 && op < 0x7f) ? op : '.');
    return 1;
}

static void disassemble(FILE *out, const char *src)
{
    if (img_min < 0) { fprintf(stderr, "gdis: empty image\n"); return; }

    /* Pass 1: collect branch-target labels. */
    for (long a = img_min; a < img_max; ) {
        if (!present[a]) { a++; continue; }
        a += decode_at(a, 1, NULL);
    }

    fprintf(out, "; Disassembly of %s\n", src ? src : "image");
    fprintf(out, "; origin 0x%04lX, %ld bytes  (re-assemble with gasm)\n\n",
            img_min, img_max - img_min);

    /* Pass 2: emit, inserting ORG at gaps and labels at targets. */
    long expect = img_min;
    fprintf(out, "        ORG    0x%04lX\n", img_min);
    for (long a = img_min; a < img_max; ) {
        if (!present[a]) { a++; continue; }
        if (a != expect) fprintf(out, "\n        ORG    0x%04lX\n", a);
        if (labels[a]) fprintf(out, "L_%04lX:\n", a);
        int len = decode_at(a, 2, out);
        a += len;
        expect = a;
    }
}

/* ------------------------------------------------------------------ */
/* Raw hex dump of the assembled image (the "depunch" raw output)      */
/* ------------------------------------------------------------------ */

static void dump_hex(FILE *out)
{
    for (long a = img_min; a < img_max; a += 16) {
        fprintf(out, "%04lX: ", a);
        for (long b = a; b < a + 16 && b < img_max; b++)
            fprintf(out, present[b] ? "%02X " : "-- ", image[b]);
        fputc('\n', out);
    }
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

static enum transcode_mode parse_mode(const char *s)
{
    if (!strcmp(s, "colbin")) return TC_COLBIN;
    if (!strcmp(s, "binary")) return TC_BINARY;
    if (!strcmp(s, "normal")) return TC_NORMAL;
    if (!strcmp(s, "hex"))    return TC_HEX;
    fprintf(stderr, "gdis: unknown mode '%s' (colbin|binary|normal|hex)\n", s);
    exit(2);
}

int main(int argc, char **argv)
{
    const char *inpath = NULL, *outpath = NULL;
    enum transcode_mode mode = TC_COLBIN;
    int is_bin = 0, loose = 0, do_cards = 0, do_hex = 0, do_image = 0, verbose = 0;
    long org = 0x0000;
    long entry = -1;   /* --entry; default = image origin (img_min) */
    uint8_t user_prefix[8]; int have_user_prefix = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc) outpath = argv[++i];
        else if (!strcmp(argv[i], "--bin")) is_bin = 1;
        else if (!strcmp(argv[i], "--org") && i + 1 < argc) org = strtol(argv[++i], NULL, 0);
        else if (!strcmp(argv[i], "--mode") && i + 1 < argc) mode = parse_mode(argv[++i]);
        else if (!strcmp(argv[i], "--loose")) loose = 1;
        else if (!strcmp(argv[i], "--cards")) do_cards = 1;
        else if (!strcmp(argv[i], "--hex")) do_hex = 1;
        else if (!strcmp(argv[i], "--image")) do_image = 1;
        else if (!strcmp(argv[i], "--entry") && i + 1 < argc) entry = strtol(argv[++i], NULL, 0);
        else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) verbose = 1;
        else if (!strcmp(argv[i], "--prefix") && i + 1 < argc) {
            /* 8 hex bytes, space/comma separated, e.g. "00 04 40 00 20 40 40 42" */
            char tmp[128]; strncpy(tmp, argv[++i], sizeof tmp - 1); tmp[sizeof tmp - 1] = '\0';
            int k = 0; char *t = strtok(tmp, " ,");
            while (t && k < 8) { user_prefix[k++] = (uint8_t)strtoul(t, NULL, 16); t = strtok(NULL, " ,"); }
            if (k != 8) { fprintf(stderr, "gdis: --prefix needs 8 hex bytes\n"); return 2; }
            have_user_prefix = 1;
        }
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            printf("Usage: gdis [options] input\n"
                   "  input is a .cap deck (default) or a raw image (--bin).\n"
                   "Options:\n"
                   "  -o FILE        write output to FILE (default stdout)\n"
                   "  --bin          treat input as a raw machine-code image\n"
                   "  --org 0xNNNN   origin for --bin / disassembly (default 0x0000)\n"
                   "  --mode M       .cap decode: colbin|binary|normal|hex (default colbin)\n"
                   "  --prefix \"HH HH ...\"  force the 8-byte data-card prefix (default: auto-detect)\n"
                   "  --loose        accept any self-consistent card record (ignore the prefix)\n"
                   "  --cards        dump per-card decoded bytes + record parse, then exit\n"
                   "  --hex          dump the reconstructed image as hex, then exit\n"
                   "  --image        write a unified-format binary (GE12 header + image)\n"
                   "  --entry 0xNNNN entry point for --image (default = image origin)\n"
                   "  -v             verbose card-by-card report on stderr\n");
            return 0;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "gdis: unknown option '%s'\n", argv[i]); return 2;
        } else inpath = argv[i];
    }
    if (!inpath) { fprintf(stderr, "gdis: no input file\n"); return 2; }

    /* --cards: inspection dump straight from the deck, then exit. */
    if (do_cards && !is_bin) {
        struct cap_deck *d = cap_load(inpath);
        if (!d) { fprintf(stderr, "gdis: cannot parse '%s'\n", inpath); return 2; }
        int nc = cap_num_cards(d);
        uint8_t want[8]; int have_want = 0;
        if (have_user_prefix) { memcpy(want, user_prefix, 8); have_want = 1; }
        else { uint8_t det[8]; if (detect_prefix(d, mode, det) >= 4) { memcpy(want, det, 8); have_want = 1; } }
        if (have_want)
            printf("# dominant data-card prefix: %02X %02X %02X %02X %02X %02X %02X %02X\n",
                   want[0],want[1],want[2],want[3],want[4],want[5],want[6],want[7]);
        for (int i = 0; i < nc; i++) {
            int ncols = cap_card_ncols(d, i);
            const uint16_t *cols = cap_card_columns(d, i);
            if (ncols < 11 || !cols) continue;
            uint8_t b[80]; int n = decode_card(cols, ncols, mode, b);
            int match = (have_want && n >= 8 && memcmp(b, want, 8) == 0);
            printf("card %3d  cols=%2d  match=%d  LL=0x%02X  addr=0x%04X  bytes:",
                   i, ncols, match, b[8], (b[9] << 8) | b[10]);
            for (int k = 0; k < n; k++) printf(" %02X", b[k]);
            putchar('\n');
        }
        cap_free(d);
        return 0;
    }

    /* Auto-detect the unified format (GE12 header) regardless of --bin/.cap:
     * gasm and `gdis --image` emit it, so a unified .bin is recognised by its
     * magic, the header stripped, and the stored origin honoured. */
    {
        FILE *pf = fopen(inpath, "rb");
        if (pf) {
            uint8_t peek[4];
            size_t got = fread(peek, 1, sizeof(peek), pf);
            if (got == 4 && peek[0] == BINIMAGE_MAGIC0 && peek[1] == BINIMAGE_MAGIC1 &&
                peek[2] == BINIMAGE_MAGIC2 && peek[3] == BINIMAGE_MAGIC3) {
                rewind(pf);
                static uint8_t ibuf[65536];
                uint16_t origin, e, len;
                int rc = binimage_read(pf, &origin, &e, ibuf, sizeof(ibuf), &len);
                fclose(pf);
                if (rc != BINIMAGE_OK) {
                    fprintf(stderr, "gdis: %s: %s\n", inpath, binimage_strerror(rc));
                    return 2;
                }
                for (uint16_t k = 0; k < len; k++) put(origin + k, ibuf[k]);
                is_bin = 1;   /* a flat image, not a .cap deck */
                fprintf(stderr, "gdis: %s: unified image 0x%04X..0x%04X (%u bytes, entry 0x%04X)\n",
                        inpath, origin, (unsigned)(origin + len - 1), (unsigned)len, e);
                goto image_loaded;
            }
            fclose(pf);
        }
    }

    /* Load the image. */
    if (is_bin) {
        FILE *f = fopen(inpath, "rb");
        if (!f) { fprintf(stderr, "gdis: cannot open '%s'\n", inpath); return 2; }
        int c; long a = org;
        while ((c = fgetc(f)) != EOF) put(a++, (uint8_t)c);
        fclose(f);
        fprintf(stderr, "gdis: %s: loaded 0x%04lX..0x%04lX (%ld bytes)\n",
                inpath, org, img_max - 1, img_max - org);
    } else {
        if (load_cap(inpath, mode, loose, verbose,
                     have_user_prefix ? user_prefix : NULL) != 0) return 1;
    }

image_loaded:;

    /* --image: write a gemu-loadable unified-format binary (GE12 header +
     * the reconstructed contiguous image img_min..img_max). origin = img_min,
     * entry = --entry or origin. Binary output mode. */
    if (do_image) {
        if (img_min < 0 || img_max <= img_min) {
            fprintf(stderr, "gdis: empty image, nothing to write\n");
            return 2;
        }
        uint16_t origin = (uint16_t)img_min;
        uint16_t length = (uint16_t)(img_max - img_min);
        uint16_t epoint = (entry >= 0) ? (uint16_t)entry : origin;
        FILE *out = outpath ? fopen(outpath, "wb") : stdout;
        if (!out) { fprintf(stderr, "gdis: cannot write '%s'\n", outpath); return 2; }
        int rc = binimage_write(out, origin, epoint, image + img_min, length);
        if (out != stdout) fclose(out);
        if (rc != BINIMAGE_OK) {
            fprintf(stderr, "gdis: cannot write image: %s\n", binimage_strerror(rc));
            return 2;
        }
        fprintf(stderr, "gdis: wrote unified image: origin 0x%04X, entry 0x%04X, %u bytes\n",
                origin, epoint, (unsigned)length);
        return 0;
    }

    FILE *out = outpath ? fopen(outpath, "w") : stdout;
    if (!out) { fprintf(stderr, "gdis: cannot write '%s'\n", outpath); return 2; }

    if (do_hex) dump_hex(out);
    else        disassemble(out, inpath);

    if (out != stdout) fclose(out);
    return 0;
}
