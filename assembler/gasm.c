/*
 * gasm — a standalone assembler for the GE-120 / GE-130 family.
 *
 * Turns GE-120 assembly source into a raw machine-code binary (pure machine
 * code, no header) that the gemu emulator can run.
 *
 * The instruction encoding is the *encode* counterpart of gemu's decoder.
 * The authoritative sources for every constant below are, in the gemu tree:
 *
 *   opcodes.h       — opcode bytes, P-format second chars, mnemonic grouping.
 *   msl-commands.c  — address field split (eff_addr / reg_addr_of), the
 *                     register-aux (N = aux & 7), the immediate-aux (= K), and
 *                     the SS length byte (single-length len=(LL&0xff)+1 vs
 *                     two-length alen=(LL>>4)+1 / blen=(LL&0xf)+1).
 *   signals.h       — branch condition mask (verified_condition): aux bits
 *                     4..7 -> cc 3,2,1,0  (0x80->cc0, 0x40->cc1, 0x20->cc2,
 *                     0x10->cc3); all-ones nibble 0xF0 = jump on any cc.
 *   ge.c            — ge_load_program() memcpy's the image to mem[0] and reset
 *                     leaves rPO = 0, so the directly-runnable convention is
 *                     ORG 0. (Card-deck/DUMP1 programs instead load at 0x0100.)
 *
 * Instruction formats (top two opcode bits select the format):
 *   P   (0x00-0x3F) : 2 bytes  [op][aux]
 *   PM  (0x40-0xBF) : 4 bytes  [op][aux][Ahi][Alo]
 *   SS  (0xC0-0xFF) : 6 bytes  [op][LL][A1hi][A1lo][A2hi][A2lo]
 * Addresses are big-endian. A 16-bit instruction address field is
 *   field = (modifier << 12) | displacement
 *   EA    = displacement + change_register[modifier]
 * (Bit 15 is the architectural absolute/modified flag — CPU[4] sec.2.5 — but the
 * modified-address indexing cycle is not yet implemented in gemu, so the
 * toolchain stays on base+displacement; see reference_operand_fetch_flowchart.)
 * With the identity change-register defaults an absolute address A <= 0x7FFF
 * encodes as field == A; higher addresses need an explicit base via disp(N).
 *
 * This file is self-contained C99 apart from binimage.h/.c (the standalone
 * shared definition of the unified output format); it includes no other gemu
 * header, so it still builds independently of the emulator.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stdarg.h>

#include "binimage.h"   /* shared unified-format writer (origin+entry header) */

/* ------------------------------------------------------------------ */
/* Instruction table                                                   */
/* ------------------------------------------------------------------ */

typedef enum {
    F_P,       /* fixed 2-byte control op, no operands           */
    F_BRANCH,  /* JC/JCC:   mask, addr                           */
    F_JU,      /* JU:       addr            (aux forced 0xF0)    */
    F_JALIAS,  /* JE/JL/...: addr           (aux = .aux mask)    */
    F_SENSE,   /* JS1/JS2/JIE: addr         (aux = .aux fixed)   */
    F_REG,     /* LR/STR/...: N, addr       (aux = N & 7)        */
    F_IMM,     /* MVI/NI/...: K, addr       (aux = K)            */
    F_PER,     /* PER/PERI/...: aux, addr   (generic PM)         */
    F_SS1,     /* MVC/...:   len, A1, A2    (LL = len-1)         */
    F_SS2      /* AP/...:    l1, l2, A1, A2 (LL=((l1-1)<<4)|(l2-1))*/
} fmt_t;

struct mnem {
    const char *name;
    uint8_t     op;
    uint8_t     aux;   /* fixed aux / second-char / alias mask    */
    fmt_t       fmt;
    int         len;   /* instruction length in bytes             */
};

/* Transcribed from opcodes.h (source of truth — keep in sync). The condition
 * masks for the jump aliases follow the cc convention documented in ISA.md §5:
 * cc1 = "<" (0x40), cc2 = "=" (0x20), cc3 = ">" (0x10), cc0 = overflow/special
 * (0x80). These aliases are an assembler convenience; the real machine only
 * has JC/JCC/JU. */
static const struct mnem MNEMS[] = {
    /* --- P format (2 bytes) --------------------------------------- */
    { "HLT",  0x0A, 0x00, F_P, 2 },
    { "NOP2", 0x07, 0x00, F_P, 2 },
    { "NOP",  0x07, 0x00, F_P, 2 },   /* convenience alias of NOP2     */
    { "ENS",  0x02, 0x10, F_P, 2 },
    { "INS",  0x02, 0x20, F_P, 2 },
    { "LOFF", 0x02, 0x40, F_P, 2 },
    { "LON",  0x02, 0x80, F_P, 2 },
    { "LOLL", 0x02, 0x91, F_P, 2 },

    /* --- PM branches (4 bytes) ------------------------------------ */
    { "JC",  0x43, 0x00, F_BRANCH, 4 },
    { "JCC", 0x40, 0x00, F_BRANCH, 4 },
    { "JU",  0x47, 0xF0, F_JU,     4 },
    { "JMP", 0x43, 0xF0, F_JALIAS, 4 },   /* = JC 0xF0 (jump always)   */
    { "JANY",0x43, 0xF0, F_JALIAS, 4 },
    { "JL",  0x43, 0x40, F_JALIAS, 4 },   /* cc1: first < second / <0  */
    { "JLT", 0x43, 0x40, F_JALIAS, 4 },
    { "JE",  0x43, 0x20, F_JALIAS, 4 },   /* cc2: equal / = 0          */
    { "JEQ", 0x43, 0x20, F_JALIAS, 4 },
    { "JZ",  0x43, 0x20, F_JALIAS, 4 },
    { "JH",  0x43, 0x10, F_JALIAS, 4 },   /* cc3: first > second / >0  */
    { "JGT", 0x43, 0x10, F_JALIAS, 4 },
    { "JNE", 0x43, 0x50, F_JALIAS, 4 },   /* cc1|cc3                   */
    { "JNZ", 0x43, 0x50, F_JALIAS, 4 },
    { "JLE", 0x43, 0x60, F_JALIAS, 4 },   /* cc1|cc2                   */
    { "JGE", 0x43, 0x30, F_JALIAS, 4 },   /* cc2|cc3                   */
    { "JOV", 0x43, 0x80, F_JALIAS, 4 },   /* cc0: overflow / special   */
    { "JS1", 0x53, 0x80, F_SENSE,  4 },
    { "JS2", 0x53, 0x40, F_SENSE,  4 },
    { "JIE", 0x53, 0x20, F_SENSE,  4 },

    /* --- PM register & address (4 bytes) -------------------------- */
    { "LR",  0xBC, 0x00, F_REG, 4 },
    { "STR", 0x84, 0x00, F_REG, 4 },
    { "LA",  0x68, 0x00, F_REG, 4 },
    { "CMR", 0xBD, 0x00, F_REG, 4 },
    { "AMR", 0xBE, 0x00, F_REG, 4 },
    { "SMR", 0xBF, 0x00, F_REG, 4 },

    /* --- PM immediate (4 bytes) ----------------------------------- */
    { "MVI", 0x92, 0x00, F_IMM, 4 },
    { "NI",  0x94, 0x00, F_IMM, 4 },
    { "CMI", 0x95, 0x00, F_IMM, 4 },
    { "CI",  0x96, 0x00, F_IMM, 4 },
    { "XI",  0x97, 0x00, F_IMM, 4 },
    { "TM",  0x91, 0x00, F_IMM, 4 },

    /* --- PM peripheral / misc (generic aux,addr; see ISA.md) ------ */
    { "PER", 0x9E, 0x00, F_PER, 4 },
    { "PERI",0x9C, 0x00, F_PER, 4 },
    { "RDC", 0x90, 0x00, F_PER, 4 },
    { "LPSR",0x9D, 0x00, F_PER, 4 },   /* opcode assigned; not wired in gemu */
    { "JRT", 0x41, 0x00, F_PER, 4 },   /* opcode assigned; not wired in gemu */

    /* --- SS single-length (6 bytes): LL = len-1, len 1..256 ------- */
    { "MVC", 0xD2, 0x00, F_SS1, 6 },
    { "NC",  0xD4, 0x00, F_SS1, 6 },
    { "CMC", 0xD5, 0x00, F_SS1, 6 },
    { "OC",  0xD6, 0x00, F_SS1, 6 },
    { "XC",  0xD7, 0x00, F_SS1, 6 },
    { "TL",  0xDC, 0x00, F_SS1, 6 },
    { "MVQ", 0xF8, 0x00, F_SS1, 6 },
    { "CMQ", 0xF9, 0x00, F_SS1, 6 },
    { "EDT", 0xDE, 0x00, F_SS1, 6 },
    /* SR/SL: real SS opcodes, but their model-byte/result-register encoding is
     * unconfirmed (ISA.md §6.10, ◑ ALU-only in gemu). Encoded here as plain
     * single-length SS for round-trip symmetry with the gdis disassembler. */
    { "SR",  0xD9, 0x00, F_SS1, 6 },
    { "SL",  0xDB, 0x00, F_SS1, 6 },

    /* --- SS two-length (6 bytes): LL=((l1-1)<<4)|(l2-1), l 1..16 -- */
    { "PK",  0xDA, 0x00, F_SS2, 6 },
    { "UPK", 0xD8, 0x00, F_SS2, 6 },
    { "PKS", 0xEE, 0x00, F_SS2, 6 },
    { "UPKS",0xEF, 0x00, F_SS2, 6 },
    { "MVP", 0xE8, 0x00, F_SS2, 6 },
    { "CMP", 0xE9, 0x00, F_SS2, 6 },
    { "AP",  0xEA, 0x00, F_SS2, 6 },
    { "SP",  0xEB, 0x00, F_SS2, 6 },
    { "MP",  0xEC, 0x00, F_SS2, 6 },
    { "DP",  0xED, 0x00, F_SS2, 6 },
    { "AD",  0xFA, 0x00, F_SS2, 6 },
    { "SD",  0xFB, 0x00, F_SS2, 6 },
    { "AB",  0xFE, 0x00, F_SS2, 6 },
    { "SB",  0xFF, 0x00, F_SS2, 6 },
};
#define NMNEMS ((int)(sizeof(MNEMS) / sizeof(MNEMS[0])))

/* ------------------------------------------------------------------ */
/* Symbol table                                                        */
/* ------------------------------------------------------------------ */

struct sym { char name[64]; long value; int defined; };
static struct sym syms[4096];
static int nsyms = 0;

static struct sym *sym_find(const char *name)
{
    for (int i = 0; i < nsyms; i++)
        if (strcmp(syms[i].name, name) == 0)
            return &syms[i];
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Error reporting                                                     */
/* ------------------------------------------------------------------ */

static const char *g_file = "<stdin>";
static int g_line = 0;
static int g_errors = 0;

static void err(const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "%s:%d: error: ", g_file, g_line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    g_errors++;
}

/* ------------------------------------------------------------------ */
/* Lexing helpers                                                      */
/* ------------------------------------------------------------------ */

/* Strip a trailing comment (';' or '#') that is not inside a string. */
static void strip_comment(char *s)
{
    int in_str = 0;
    for (char *p = s; *p; p++) {
        if (*p == '"') in_str = !in_str;
        else if (!in_str && (*p == ';' || *p == '#')) { *p = '\0'; return; }
    }
}

static char *trim(char *s)
{
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s) return s;
    char *e = s + strlen(s) - 1;
    while (e > s && isspace((unsigned char)*e)) *e-- = '\0';
    return s;
}

static void upper(char *s) { for (; *s; s++) *s = (char)toupper((unsigned char)*s); }

/* Split a string on top-level commas (commas not inside () or ""). Returns the
 * number of fields; trims each field in place into out[]. */
static int split_commas(char *s, char *out[], int max)
{
    int n = 0, depth = 0, in_str = 0;
    char *start = s;
    for (char *p = s; ; p++) {
        if (*p == '"') in_str = !in_str;
        else if (!in_str && *p == '(') depth++;
        else if (!in_str && *p == ')') depth--;
        if ((*p == ',' && depth == 0 && !in_str) || *p == '\0') {
            int last = (*p == '\0');
            *p = '\0';
            if (n < max) out[n++] = trim(start);
            start = p + 1;
            if (last) break;
        }
    }
    /* a single empty field means "no operands" */
    if (n == 1 && out[0][0] == '\0') return 0;
    return n;
}

/* ------------------------------------------------------------------ */
/* Expression evaluation                                               */
/* ------------------------------------------------------------------ */

/* Evaluate a single term: hex (0x.. / $..), decimal, char 'c', or symbol. */
static long eval_term(const char *t, int pass, int *ok)
{
    *ok = 1;
    if (t[0] == '\0') { err("empty expression term"); *ok = 0; return 0; }

    if (t[0] == '\'' ) {                       /* character literal */
        if (t[1] && t[2] == '\'') return (unsigned char)t[1];
        err("malformed character literal '%s'", t);
        *ok = 0; return 0;
    }
    if (t[0] == '0' && (t[1] == 'x' || t[1] == 'X'))
        return strtol(t + 2, NULL, 16);
    if (t[0] == '$')
        return strtol(t + 1, NULL, 16);
    if (isdigit((unsigned char)t[0]) ||
        ((t[0] == '+' || t[0] == '-') && isdigit((unsigned char)t[1])))
        return strtol(t, NULL, 10);

    /* symbol */
    struct sym *s = sym_find(t);
    if (s && s->defined) return s->value;
    if (pass == 2) { err("undefined symbol '%s'", t); *ok = 0; }
    return 0;   /* pass 1: forward reference, size is fixed anyway */
}

/* Evaluate an expression of the form term (('+'|'-') term)* . */
static long eval_expr(const char *expr, int pass, int *ok)
{
    char buf[256];
    strncpy(buf, expr, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    long acc = 0;
    int sign = 1, have = 0;
    char *p = buf;
    char tok[128];
    *ok = 1;

    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        if (have && (*p == '+' || *p == '-')) {
            sign = (*p == '-') ? -1 : 1;
            p++;
            continue;
        }
        /* gather a term up to the next top-level +/- (but keep a leading
         * sign attached to a number/char literal handled by eval_term) */
        int i = 0;
        while (*p && *p != '+' && *p != '-' && !isspace((unsigned char)*p))
            tok[i++] = *p++;
        tok[i] = '\0';
        int tok_ok;
        long v = eval_term(tok, pass, &tok_ok);
        if (!tok_ok) *ok = 0;
        acc += sign * v;
        sign = 1;
        have = 1;
    }
    if (!have) { err("empty expression"); *ok = 0; }
    return acc;
}

/* ------------------------------------------------------------------ */
/* Address-field encoding                                              */
/* ------------------------------------------------------------------ */

/* Parse a memory address operand into a 16-bit instruction field.
 *   "expr"        -> absolute; field = value (must be <= 0x7FFF)
 *   "disp(N)"     -> field = ((N & 7) << 12) | (disp & 0xFFF)
 *                    EA = change_register[N] + disp
 */
static int parse_addr(const char *operand, int pass, uint16_t *field)
{
    char buf[256];
    strncpy(buf, operand, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *lp = strchr(buf, '(');
    if (lp) {
        char *rp = strchr(lp, ')');
        if (!rp) { err("missing ')' in address '%s'", operand); return -1; }
        *lp = '\0'; *rp = '\0';
        int ok1, ok2;
        long disp = eval_expr(buf, pass, &ok1);
        long n    = eval_expr(lp + 1, pass, &ok2);
        if (pass == 2 && (!ok1 || !ok2)) return -1;
        if (pass == 2) {
            if (n < 0 || n > 7) { err("base register %ld out of range 0..7", n); return -1; }
            if (disp < 0 || disp > 0xFFF) { err("displacement 0x%lX out of range 0..0xFFF", disp); return -1; }
        }
        *field = (uint16_t)(((n & 7) << 12) | (disp & 0xFFF));
        return 0;
    }

    int ok;
    long v = eval_expr(buf, pass, &ok);
    if (pass == 2 && !ok) return -1;
    if (pass == 2 && (v < 0 || v > 0x7FFF)) {
        err("address 0x%lX exceeds 0x7FFF; reach it via disp(N) against a reprogrammed base", v);
        return -1;
    }
    *field = (uint16_t)(v & 0x7FFF);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Output image                                                        */
/* ------------------------------------------------------------------ */

static uint8_t image[65536];
static long img_min = -1;     /* lowest address written */
static long img_max = 0;      /* one past highest address written */
static long g_entry = -1;     /* entry point (ENTRY directive); <0 = default */

static void emit_byte(long addr, uint8_t b)
{
    if (addr < 0 || addr > 0xFFFF) { err("address 0x%lX out of memory", addr); return; }
    image[addr] = b;
    if (img_min < 0 || addr < img_min) img_min = addr;
    if (addr + 1 > img_max) img_max = addr + 1;
}

/* ------------------------------------------------------------------ */
/* Assembly                                                            */
/* ------------------------------------------------------------------ */

static const struct mnem *find_mnem(const char *name)
{
    for (int i = 0; i < NMNEMS; i++)
        if (strcmp(MNEMS[i].name, name) == 0)
            return &MNEMS[i];
    return NULL;
}

/* Length of a directive's emitted data, or -1 if line is not a directive. */
static long directive_size(const char *op, char *args)
{
    char tmp[1024];
    char *fields[256];

    if (strcmp(op, "ORG") == 0 || strcmp(op, "EQU") == 0 ||
        strcmp(op, "ENTRY") == 0) return 0;
    if (strcmp(op, "DS") == 0) {
        int ok; long n = eval_expr(args, 1, &ok); return n < 0 ? 0 : n;
    }
    if (strcmp(op, "DW") == 0) {
        strncpy(tmp, args, sizeof(tmp) - 1); tmp[sizeof(tmp)-1] = '\0';
        return 2 * split_commas(tmp, fields, 256);
    }
    if (strcmp(op, "DB") == 0) {
        long total = 0;
        strncpy(tmp, args, sizeof(tmp) - 1); tmp[sizeof(tmp)-1] = '\0';
        int nf = split_commas(tmp, fields, 256);
        for (int i = 0; i < nf; i++) {
            if (fields[i][0] == '"') {
                long len = (long)strlen(fields[i]);
                /* string content excludes the two surrounding quotes */
                total += (len >= 2) ? len - 2 : 0;
            } else total += 1;
        }
        return total;
    }
    return -1;
}

/* Emit a directive's bytes at lc (pass 2). */
static void emit_directive(const char *op, char *args, long lc)
{
    char tmp[1024];
    char *fields[256];

    if (strcmp(op, "DS") == 0) {
        int ok; long n = eval_expr(args, 2, &ok);
        for (long i = 0; i < n; i++) emit_byte(lc + i, 0);
        return;
    }
    if (strcmp(op, "DW") == 0) {
        strncpy(tmp, args, sizeof(tmp) - 1); tmp[sizeof(tmp)-1] = '\0';
        int nf = split_commas(tmp, fields, 256);
        for (int i = 0; i < nf; i++) {
            int ok; long v = eval_expr(fields[i], 2, &ok);
            emit_byte(lc++, (uint8_t)((v >> 8) & 0xff));
            emit_byte(lc++, (uint8_t)(v & 0xff));
        }
        return;
    }
    if (strcmp(op, "DB") == 0) {
        strncpy(tmp, args, sizeof(tmp) - 1); tmp[sizeof(tmp)-1] = '\0';
        int nf = split_commas(tmp, fields, 256);
        for (int i = 0; i < nf; i++) {
            if (fields[i][0] == '"') {
                char *q = fields[i] + 1;
                long len = (long)strlen(q);
                if (len > 0 && q[len-1] == '"') q[len-1] = '\0';
                for (char *c = q; *c; c++) emit_byte(lc++, (uint8_t)*c);
            } else {
                int ok; long v = eval_expr(fields[i], 2, &ok);
                emit_byte(lc++, (uint8_t)(v & 0xff));
            }
        }
        return;
    }
}

/* Encode one instruction at lc (pass 2). */
static void emit_instr(const struct mnem *m, char *args, long lc)
{
    char *f[8];
    int nf = split_commas(args, f, 8);
    uint16_t field;

    switch (m->fmt) {
    case F_P:
        if (nf != 0) err("%s takes no operands", m->name);
        emit_byte(lc, m->op);
        emit_byte(lc + 1, m->aux);
        break;

    case F_JU:
    case F_JALIAS:
    case F_SENSE:
        if (nf != 1) { err("%s expects 1 operand (addr)", m->name); break; }
        if (parse_addr(f[0], 2, &field)) break;
        emit_byte(lc, m->op);
        emit_byte(lc + 1, m->aux);
        emit_byte(lc + 2, (uint8_t)(field >> 8));
        emit_byte(lc + 3, (uint8_t)(field & 0xff));
        break;

    case F_BRANCH: {
        if (nf != 2) { err("%s expects 2 operands (mask, addr)", m->name); break; }
        int ok; long mask = eval_expr(f[0], 2, &ok);
        if (parse_addr(f[1], 2, &field)) break;
        if (mask < 0 || mask > 0xFF) err("%s mask 0x%lX out of range", m->name, mask);
        emit_byte(lc, m->op);
        emit_byte(lc + 1, (uint8_t)(mask & 0xF0));   /* only bits 4..7 matter */
        emit_byte(lc + 2, (uint8_t)(field >> 8));
        emit_byte(lc + 3, (uint8_t)(field & 0xff));
        break;
    }

    case F_REG: {
        if (nf != 2) { err("%s expects 2 operands (N, addr)", m->name); break; }
        int ok; long n = eval_expr(f[0], 2, &ok);
        if (parse_addr(f[1], 2, &field)) break;
        if (n < 0 || n > 7) err("%s register %ld out of range 0..7", m->name, n);
        emit_byte(lc, m->op);
        emit_byte(lc + 1, (uint8_t)(n & 7));
        emit_byte(lc + 2, (uint8_t)(field >> 8));
        emit_byte(lc + 3, (uint8_t)(field & 0xff));
        break;
    }

    case F_IMM:
    case F_PER: {
        if (nf != 2) { err("%s expects 2 operands (%s, addr)", m->name,
                           m->fmt == F_IMM ? "K" : "aux"); break; }
        int ok; long k = eval_expr(f[0], 2, &ok);
        if (parse_addr(f[1], 2, &field)) break;
        if (k < 0 || k > 0xFF) err("%s immediate 0x%lX out of range 0..0xFF", m->name, k);
        emit_byte(lc, m->op);
        emit_byte(lc + 1, (uint8_t)(k & 0xff));
        emit_byte(lc + 2, (uint8_t)(field >> 8));
        emit_byte(lc + 3, (uint8_t)(field & 0xff));
        break;
    }

    case F_SS1: {
        if (nf != 3) { err("%s expects 3 operands (len, A1, A2)", m->name); break; }
        int ok; long len = eval_expr(f[0], 2, &ok);
        uint16_t a1, a2;
        if (parse_addr(f[1], 2, &a1)) break;
        if (parse_addr(f[2], 2, &a2)) break;
        if (len < 1 || len > 256) { err("%s length %ld out of range 1..256", m->name, len); break; }
        emit_byte(lc,     m->op);
        emit_byte(lc + 1, (uint8_t)((len - 1) & 0xff));
        emit_byte(lc + 2, (uint8_t)(a1 >> 8));
        emit_byte(lc + 3, (uint8_t)(a1 & 0xff));
        emit_byte(lc + 4, (uint8_t)(a2 >> 8));
        emit_byte(lc + 5, (uint8_t)(a2 & 0xff));
        break;
    }

    case F_SS2: {
        if (nf != 4) { err("%s expects 4 operands (l1, l2, A1, A2)", m->name); break; }
        int ok; long l1 = eval_expr(f[0], 2, &ok), l2 = eval_expr(f[1], 2, &ok);
        uint16_t a1, a2;
        if (parse_addr(f[2], 2, &a1)) break;
        if (parse_addr(f[3], 2, &a2)) break;
        if (l1 < 1 || l1 > 16) { err("%s l1 %ld out of range 1..16", m->name, l1); break; }
        if (l2 < 1 || l2 > 16) { err("%s l2 %ld out of range 1..16", m->name, l2); break; }
        emit_byte(lc,     m->op);
        emit_byte(lc + 1, (uint8_t)((((l1 - 1) & 0xf) << 4) | ((l2 - 1) & 0xf)));
        emit_byte(lc + 2, (uint8_t)(a1 >> 8));
        emit_byte(lc + 3, (uint8_t)(a1 & 0xff));
        emit_byte(lc + 4, (uint8_t)(a2 >> 8));
        emit_byte(lc + 5, (uint8_t)(a2 & 0xff));
        break;
    }
    }
}

/* ------------------------------------------------------------------ */
/* Line storage (so we can run two passes without re-reading the file) */
/* ------------------------------------------------------------------ */

struct line { char *text; int lineno; };
static struct line lines[65536];
static int nlines = 0;

/* Parse a logical line into: optional label, opcode token, and the rest
 * (arguments). Returns 1 if there is an opcode, 0 if the line is label-only or
 * blank. The input buffer is modified in place. */
static int parse_line(char *s, char **label, char **op, char **args)
{
    *label = NULL; *op = NULL; *args = NULL;
    s = trim(s);
    if (!*s) return 0;

    /* label: a leading token ending in ':' (or, for EQU, a bare name) */
    char *colon = NULL;
    for (char *p = s; *p && !isspace((unsigned char)*p); p++)
        if (*p == ':') { colon = p; break; }
    if (colon) {
        *colon = '\0';
        *label = s;
        s = trim(colon + 1);
        if (!*s) return 0;
    }

    /* opcode token */
    char *p = s;
    while (*p && !isspace((unsigned char)*p)) p++;
    if (*p) { *p = '\0'; *args = trim(p + 1); } else { *args = p; }
    *op = s;

    /* EQU special case: "NAME EQU expr" — first token is the symbol name */
    if (*args) {
        char *a = *args;
        /* peek the first token of args to detect EQU */
        char abuf[64]; int i = 0;
        while (a[i] && !isspace((unsigned char)a[i]) && i < 63) { abuf[i] = (char)toupper((unsigned char)a[i]); i++; }
        abuf[i] = '\0';
        if (strcmp(abuf, "EQU") == 0) {
            /* op is actually the symbol name; rebuild */
            *label = *op;          /* treat name as a label-style definition */
            *op = NULL;            /* signal: handled as EQU below */
            *args = trim(a + i);
            return 2;              /* 2 = EQU line */
        }
    }
    return 1;
}

int main(int argc, char **argv)
{
    const char *inpath = NULL, *outpath = "a.bin", *listpath = NULL;
    long org = 0x0000;
    int raw_out = 0;   /* --raw: emit a headerless flat image (old behaviour) */

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) outpath = argv[++i];
        else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) listpath = argv[++i];
        else if (strcmp(argv[i], "--org") == 0 && i + 1 < argc) org = strtol(argv[++i], NULL, 0);
        else if (strcmp(argv[i], "--raw") == 0) raw_out = 1;
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: gasm [-o out.bin] [-l listing.txt] [--org 0xNNNN] "
                   "[--raw] input.s\n"
                   "  Default output: unified format (GE12 header + image).\n"
                   "  --raw          : headerless flat image.\n"
                   "  ENTRY <expr>   : source directive sets the entry point\n"
                   "                   (default = load origin).\n");
            return 0;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "gasm: unknown option '%s'\n", argv[i]);
            return 2;
        } else inpath = argv[i];
    }
    if (!inpath) { fprintf(stderr, "gasm: no input file\n"); return 2; }

    FILE *fp = fopen(inpath, "r");
    if (!fp) { fprintf(stderr, "gasm: cannot open '%s'\n", inpath); return 2; }
    g_file = inpath;

    /* slurp lines */
    char raw[1024];
    int ln = 0;
    while (fgets(raw, sizeof(raw), fp)) {
        ln++;
        char *copy = malloc(strlen(raw) + 1);
        strcpy(copy, raw);
        lines[nlines].text = copy;
        lines[nlines].lineno = ln;
        nlines++;
    }
    fclose(fp);

    /* ---- Pass 1: build symbol table, compute addresses ---- */
    long lc = org;
    for (int i = 0; i < nlines; i++) {
        char work[1024];
        strncpy(work, lines[i].text, sizeof(work) - 1);
        work[sizeof(work) - 1] = '\0';
        g_line = lines[i].lineno;
        strip_comment(work);

        char *label, *op, *args;
        int kind = parse_line(work, &label, &op, &args);

        if (kind == 2) {   /* EQU */
            int ok; long v = eval_expr(args, 1, &ok);
            if (label) {
                struct sym *s = sym_find(label);
                if (!s) { s = &syms[nsyms++]; strncpy(s->name, label, 63); }
                s->value = v; s->defined = 1;
            }
            continue;
        }

        if (label) {       /* label definition at current lc */
            struct sym *s = sym_find(label);
            if (!s) { s = &syms[nsyms++]; strncpy(s->name, label, 63); }
            s->value = lc; s->defined = 1;
        }
        if (kind == 0) continue;   /* label-only / blank */

        char opu[64];
        strncpy(opu, op, 63); opu[63] = '\0'; upper(opu);

        if (strcmp(opu, "ORG") == 0) {
            int ok; lc = eval_expr(args, 1, &ok);
            continue;
        }
        long dsz = directive_size(opu, args);
        if (dsz >= 0) { lc += dsz; continue; }

        const struct mnem *m = find_mnem(opu);
        if (!m) { err("unknown mnemonic '%s'", op); continue; }
        lc += m->len;
    }

    /* ---- Pass 2: emit bytes ---- */
    lc = org;
    for (int i = 0; i < nlines; i++) {
        char work[1024];
        strncpy(work, lines[i].text, sizeof(work) - 1);
        work[sizeof(work) - 1] = '\0';
        g_line = lines[i].lineno;
        strip_comment(work);

        char *label, *op, *args;
        int kind = parse_line(work, &label, &op, &args);
        if (kind == 2) continue;          /* EQU already handled */
        if (kind == 0) continue;          /* label-only / blank */

        char opu[64];
        strncpy(opu, op, 63); opu[63] = '\0'; upper(opu);

        if (strcmp(opu, "ORG") == 0) {
            int ok; lc = eval_expr(args, 2, &ok);
            continue;
        }
        if (strcmp(opu, "ENTRY") == 0) {
            int ok; long v = eval_expr(args, 2, &ok);
            if (ok) g_entry = v;
            continue;
        }
        long dsz = directive_size(opu, args);
        if (dsz >= 0) { emit_directive(opu, args, lc); lc += dsz; continue; }

        const struct mnem *m = find_mnem(opu);
        if (!m) continue;                 /* already reported in pass 1 */
        emit_instr(m, args, lc);
        lc += m->len;
    }

    if (g_errors) {
        fprintf(stderr, "gasm: %d error(s); no output written\n", g_errors);
        return 1;
    }

    if (img_min < 0) { fprintf(stderr, "gasm: empty program\n"); return 1; }

    /* Warn if the image overlaps the change-register window 0x00F0-0x00FF. */
    if (img_min < 0x100 && img_max > 0xF0)
        fprintf(stderr, "gasm: warning: image spans 0x00F0-0x00FF "
                        "(change-register area); it may clobber segment bases\n");

    /* Output. Default: unified format (GE12 header + image). The header
     * carries origin (lowest address written) and entry (ENTRY directive, or
     * the origin by default) so gemu / the real bootstrap know where to place
     * the image and where to start. --raw keeps the old headerless flat image. */
    uint16_t origin = (uint16_t)img_min;
    uint16_t length = (uint16_t)(img_max - img_min);
    uint16_t entry  = (g_entry >= 0) ? (uint16_t)g_entry : origin;

    FILE *out = fopen(outpath, "wb");
    if (!out) { fprintf(stderr, "gasm: cannot write '%s'\n", outpath); return 2; }
    if (raw_out) {
        fwrite(image + img_min, 1, (size_t)length, out);
    } else {
        int rc = binimage_write(out, origin, entry, image + img_min, length);
        if (rc != BINIMAGE_OK) {
            fprintf(stderr, "gasm: cannot write image: %s\n", binimage_strerror(rc));
            fclose(out);
            return 2;
        }
    }
    fclose(out);

    /* optional listing */
    if (listpath) {
        FILE *lf = fopen(listpath, "w");
        if (lf) {
            fprintf(lf, "; gasm listing for %s  (origin 0x%04lX, %ld bytes)\n",
                    inpath, img_min, img_max - img_min);
            for (long a = img_min; a < img_max; a += 16) {
                fprintf(lf, "%04lX: ", a);
                for (long b = a; b < a + 16 && b < img_max; b++)
                    fprintf(lf, "%02X ", image[b]);
                fputc('\n', lf);
            }
            fclose(lf);
        }
    }

    printf("gasm: wrote %ld bytes to %s (origin 0x%04lX)\n",
           img_max - img_min, outpath, img_min);
    return 0;
}
