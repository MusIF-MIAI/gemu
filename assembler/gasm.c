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
 *                     register-aux (1XXX0000: N = (aux>>4)&7, emitted as
 *                     0x80|(N<<4)), the immediate-aux (= K), and
 *                     the SS length byte (single-length len=(LL&0xff)+1 vs
 *                     two-length alen=(LL>>4)+1 / blen=(LL&0xf)+1).
 *   signals.h       — branch condition mask (verified_condition): aux bits
 *                     4..7 -> cc 3,2,1,0  (0x80->cc0, 0x40->cc1, 0x20->cc2,
 *                     0x10->cc3); all-ones nibble 0xF0 = jump on any cc.
 *   ge.c            — the IPL nibble-packs one card to mem[0] and reset
 *                     leaves rPO = 0, so the directly-runnable convention is
 *                     ORG 0. (Card-deck/DUMP1 programs instead load at 0x0100.)
 *
 * Instruction formats (top two opcode bits select the format):
 *   P   (0x00-0x3F) : 2 bytes  [op][aux]
 *   PM  (0x40-0xBF) : 4 bytes  [op][aux][Ahi][Alo]
 *   SS  (0xC0-0xFF) : 6 bytes  [op][LL][A1hi][A1lo][A2hi][A2lo]
 * Addresses are big-endian. Bit 15 of a 16-bit address field is the
 * architectural absolute/modified flag (CPU[4] sec.2.5), honored by gemu's
 * operand-fetch indexing micro-cycle:
 *   absolute  (bit 15 = 0): field = address (<= 0x7FFF); EA = field, used directly
 *   modified  (bit 15 = 1): field = 0x8000 | (N<<12) | disp; EA = chgreg[N] + disp
 * So an absolute address A <= 0x7FFF encodes as field == A (no base added);
 * higher memory is reached relative to a reprogrammed base via disp(N).
 *
 * This file is self-contained C99 and includes no gemu header, so it builds
 * independently of the emulator. Its output is always a .cap card deck: that
 * is the only thing a GE-120 can be handed a program on.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stdarg.h>


/* ------------------------------------------------------------------ */
/* Instruction table                                                   */
/* ------------------------------------------------------------------ */

typedef enum {
    F_P,       /* fixed 2-byte control op, no operands           */
    F_BRANCH,  /* JC/JCC:   mask, addr                           */
    F_JU,      /* JU:       addr            (aux forced 0xF0)    */
    F_JALIAS,  /* JE/JL/...: addr           (aux = .aux mask)    */
    F_SENSE,   /* JS1/JS2/JIE: addr         (aux = .aux fixed)   */
    F_REG,     /* LR/STR/...: N, addr       (aux = 0x80|(N<<4))  */
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
    { "STR", 0xB4, 0x00, F_REG, 4 },
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
 *   "expr"        -> absolute; bit 15 = 0; field = value (must be <= 0x7FFF);
 *                    EA = value, used directly (no change register).
 *   "disp(N)"     -> modified; bit 15 = 1; field = 0x8000|((N&7)<<12)|(disp&0xFFF);
 *                    EA = change_register[N] + disp.
 * Bit 15 is the absolute/modified flag (CPU[4] §2.5, FO.19-20; flow chart dwg
 * 14023130). gemu resolves it in operand fetch: absolute fields verbatim,
 * modified fields via the ED|EC->EF|EE indexing micro-cycle.
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
        *field = (uint16_t)(0x8000u | ((n & 7) << 12) | (disp & 0xFFF));
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
        /* Register-op aux char is 1XXX0000: register N lives in bits 4-6
         * (ISA.md §0.5 row 3; emulator reg_addr_of reads (aux>>4)&7, disasm
         * decodes (aux>>4)&7).  Emit 1<<7 | (N<<4), e.g. R6 -> 0xE0, R7 -> 0xF0. */
        emit_byte(lc + 1, (uint8_t)(0x80 | ((n & 7) << 4)));
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

/* ------------------------------------------------------------------ */
/* Assembly driver: slurp / pass 1 / pass 2, reusable across units     */
/* (--boot assembles the program, then the boot.s template).           */
/* ------------------------------------------------------------------ */

static int asm_slurp(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp)
        return -1;
    g_file = path;
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
    return 0;
}

static void asm_reset(void)
{
    for (int i = 0; i < nlines; i++)
        free(lines[i].text);
    nlines  = 0;
    nsyms   = 0;
    g_errors = 0;
    g_entry = -1;
    img_min = -1;
    img_max = 0;
    memset(image, 0, sizeof(image));
}

static void sym_override(const char *name, long v)
{
    struct sym *s = sym_find(name);
    if (!s) {
        s = &syms[nsyms++];
        strncpy(s->name, name, 63);
        s->name[63] = 0;
    }
    s->value = v;
    s->defined = 1;
}

static void asm_pass1(long org);
static void asm_pass2(long org);

/* ------------------------------------------------------------------ */
/* .cap deck writer (--boot): boot card hex-encoded + body in COLBIN.  */
/* Column encodings are the exact inverses of the reader transcoder    */
/* (gemu transcode.c / rpi-pico-card-reader transcode.c).              */
/* ------------------------------------------------------------------ */

static uint16_t cap_hexcol(unsigned n)   /* IPL hex: nibble = row sum */
{
    if (!n)
        return 0;
    if (n <= 9)
        return (uint16_t)(1u << n);
    return (uint16_t)((1u << 9) | (1u << (n - 9)));
}

static uint16_t cap_colbin(uint8_t b)    /* by-pass: byte bit i -> row B2R[i] */
{
    static const int b2r[8] = {9, 8, 7, 6, 3, 2, 1, 0};
    uint16_t col = 0;
    for (int i = 0; i < 8; i++)
        if (b & (1u << i))
            col |= (uint16_t)(1u << b2r[i]);
    return col;
}

static void cap_write_card(FILE *f, int num, const uint16_t *cols)
{
    fprintf(f, "Card n. %d\n", num);
    for (int j = 0; j < 80; j++)
        fprintf(f, "%04x%c", cols[j], j == 79 ? '\n' : ' ');
}

/* --bootge: the ORIGINAL one-card IPL scatter loader (unit 0x80), verbatim
 * from the funktionalcpu SAT deck (bench-proven on the real machine). It
 * reads each following card to 0x0036 and relocates LL+1 bytes (byte 8 =
 * LL = len-1, bytes 9-10 = load address BE, payload from byte 11). */
static const uint16_t ge_loader_cols[80] = {
    0x0200, 0x0140, 0x0100, 0x0001, 0x0001, 0x0001, 0x0004, 0x0004,
    0x0200, 0x0140, 0x0100, 0x0001, 0x0001, 0x0001, 0x0004, 0x0001,
    0x0200, 0x0140, 0x0100, 0x0001, 0x0001, 0x0001, 0x0004, 0x0040,
    0x0010, 0x1008, 0x0002, 0x0001, 0x0001, 0x0001, 0x0002, 0x0020,
    0x0120, 0x0004, 0x0001, 0x0004, 0x0001, 0x0001, 0x0002, 0x0080,
    0x0001, 0x0001, 0x0008, 0x0140, 0x0120, 0x0004, 0x0001, 0x0001,
    0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0010, 0x0002,
    0x0010, 0x0008, 0x0180, 0x0001, 0x0001, 0x0001, 0x0001, 0x0010,
    0x0001, 0x0001, 0x0010, 0x0001, 0x0100, 0x0001, 0x0104, 0x0001,
    0x0001, 0x0001, 0x0008, 0x0040, 0x0110, 0x0001, 0x0010, 0x0104,
};

/* One scatter card for the original loader: LL/II header + payload. */
static void cap_scatter_card(uint16_t *cols, uint16_t addr,
                             const uint8_t *p, unsigned len)
{
    memset(cols, 0, 80 * sizeof(uint16_t));
    cols[8]  = cap_colbin((uint8_t)(len - 1));
    cols[9]  = cap_colbin((uint8_t)(addr >> 8));
    cols[10] = cap_colbin((uint8_t)addr);
    for (unsigned i = 0; i < len; i++)
        cols[11 + i] = cap_colbin(p[i]);
}

int main(int argc, char **argv)
{
    const char *inpath = NULL, *outpath = "a.cap", *listpath = NULL;
    long org = 0x0000;
    int card_out = 0;  /* --card: one IPL boot card (ORG 0, <=40 bytes)      */
    int boot_out = 0;  /* --boot: link with boot.s, emit a .cap deck         */
    int bootge_out = 0;/* --bootge: .cap deck with the ORIGINAL IPL loader   */

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) outpath = argv[++i];
        else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) listpath = argv[++i];
        else if (strcmp(argv[i], "--org") == 0 && i + 1 < argc) org = strtol(argv[++i], NULL, 0);
        else if (strcmp(argv[i], "--card") == 0) card_out = 1;
        else if (strcmp(argv[i], "--boot") == 0) boot_out = 1;
        else if (strcmp(argv[i], "--bootge") == 0) bootge_out = 1;
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: gasm [-o out.cap] [-l listing.txt] [--org 0xNNNN] "
                   "[--card|--boot] input.s\n"
                   "\n"
                   "  Output is always a .cap card deck -- the only thing a\n"
                   "  GE-120 can be given a program on. Run it with\n"
                   "  'ge out.cap' in the emulator, or 'arm out.cap' on the\n"
                   "  rpi-pico-card-reader against the real machine.\n"
                   "\n"
                   "  (default)      : deck carried by the ORIGINAL one-card IPL\n"
                   "                   scatter loader, embedded verbatim from the\n"
                   "                   SAT decks and bench-proven on the real\n"
                   "                   machine: 66-byte LL/II cards plus a\n"
                   "                   jump-to-origin termination card. Needs\n"
                   "                   ORG 0x0086 or above (the loader and its\n"
                   "                   card buffer live below).\n"
                   "  --boot         : same idea, but the deck leads with the\n"
                   "                   boot.s template instead (DEST/DONE/NCARDS\n"
                   "                   patched to the program) and the program\n"
                   "                   follows as raw 80-byte body cards.\n"
                   "  --card         : ONE IPL boot card. The machine reads a\n"
                   "                   single 80-column card, nibble-packs it to\n"
                   "                   40 bytes at 0x0000 and executes it there,\n"
                   "                   so the image must ORG at 0x0000 and fit\n"
                   "                   40 bytes. For anything larger, use the\n"
                   "                   default loader deck.\n"
                   "  ENTRY <expr>   : source directive sets the entry point\n"
                   "                   (default = load origin; the deck paths\n"
                   "                   enter at the origin regardless).\n");
            return 0;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "gasm: unknown option '%s'\n", argv[i]);
            return 2;
        } else inpath = argv[i];
    }
    if (!inpath) { fprintf(stderr, "gasm: no input file\n"); return 2; }
    if ((boot_out && card_out) || (bootge_out && (boot_out || card_out))) {
        fprintf(stderr, "gasm: --card and --boot are mutually exclusive\n");
        return 2;
    }
    /* No mode flag: the default IS the loader deck. --bootge is kept as an
     * explicit spelling of it, since that is what the flag used to mean. */
    if (!boot_out && !card_out)
        bootge_out = 1;

    if (asm_slurp(inpath)) {
        fprintf(stderr, "gasm: cannot open '%s'\n", inpath);
        return 2;
    }
    asm_pass1(org);
    asm_pass2(org);

    if (g_errors) {
        fprintf(stderr, "gasm: %d error(s); no output written\n", g_errors);
        return 1;
    }

    if (img_min < 0) { fprintf(stderr, "gasm: empty program\n"); return 1; }

    /* Warn if the image overlaps the change-register window 0x00F0-0x00FF. */
    if (img_min < 0x100 && img_max > 0xF0)
        fprintf(stderr, "gasm: warning: image spans 0x00F0-0x00FF "
                        "(change-register area); it may clobber segment bases\n");

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


    if (bootge_out) {
        /* ---- Original IPL scatter-loader deck ------------------------ */
        long porg = img_min, plen = img_max - img_min;
        if (g_entry >= 0 && g_entry != porg)
            fprintf(stderr, "gasm: warning: --bootge ignores ENTRY 0x%04lX "
                            "(the loader's final card jumps to the origin "
                            "0x%04lX)\n", g_entry, porg);
        if (porg < 0x0086) {
            fprintf(stderr, "gasm: --bootge: origin 0x%04lX overlaps the "
                            "loader (0x0000-0x0027) or its card buffer "
                            "(0x0036-0x0085); ORG at 0x0086 or above\n", porg);
            return 1;
        }
        FILE *out = fopen(outpath, "w");
        if (!out) { fprintf(stderr, "gasm: cannot write '%s'\n", outpath); return 2; }
        cap_write_card(out, 1, ge_loader_cols);
        uint16_t cols[80];
        long ncards = 0, off = 0;
        while (off < plen) {
            unsigned take = plen - off > 66 ? 66 : (unsigned)(plen - off);
            cap_scatter_card(cols, (uint16_t)(porg + off),
                             image + porg + off, take);
            cap_write_card(out, (int)++ncards + 1, cols);
            off += take;
        }
        /* Termination, verbatim the SAT decks' own final-card pattern:
         * NOP2, NOP2, jump-always to the origin, laid over the loader head
         * so its closing JU 0x0004 falls into the jump. */
        const uint8_t term[8] = {
            0x07, 0x00, 0x07, 0x00,
            0x43, 0xF0, (uint8_t)(porg >> 8), (uint8_t)porg,
        };
        cap_scatter_card(cols, 0x0000, term, sizeof(term));
        cap_write_card(out, (int)ncards + 2, cols);
        fclose(out);
        printf("gasm: bootge deck %s: GE loader + %ld scatter cards + "
               "termination, load+entry 0x%04lX (arm %s)\n",
               outpath, ncards, porg, outpath);
        return 0;
    }

    if (boot_out) {
        /* ---- Stage 2: link with the boot.s template ------------------ */
        static uint8_t prog_img[65536];
        long porg = img_min, plen = img_max - img_min;
        memcpy(prog_img, image + img_min, (size_t)plen);
        if (g_entry >= 0 && g_entry != porg)
            fprintf(stderr, "gasm: warning: --boot ignores ENTRY 0x%04lX "
                            "(the boot card enters at the origin 0x%04lX)\n",
                    g_entry, porg);
        long ncards = (plen + 79) / 80;
        long done   = porg + ncards * 80;
        if (porg < 0x26)
            fprintf(stderr, "gasm: warning: origin 0x%04lX overlaps the "
                            "running boot card (0x0000-0x0025)\n", porg);
        if (done > 0x10000) {
            fprintf(stderr, "gasm: --boot: image runs past the address "
                            "space (done = 0x%lX)\n", done);
            return 1;
        }

        /* boot.s lives next to the gasm binary (assembler/boot.s). */
        char bootpath[1024];
        const char *slash = strrchr(argv[0], '/');
        if (slash)
            snprintf(bootpath, sizeof(bootpath), "%.*s/boot.s",
                     (int)(slash - argv[0]), argv[0]);
        else
            snprintf(bootpath, sizeof(bootpath), "boot.s");

        asm_reset();
        if (asm_slurp(bootpath) && asm_slurp("boot.s")) {
            fprintf(stderr, "gasm: cannot open boot template '%s'\n", bootpath);
            return 2;
        }
        asm_pass1(0);
        /* The link: inject the program's geometry over the template's
         * standalone defaults (pass 2 skips EQU lines, so these win). */
        sym_override("DEST",   porg);
        sym_override("DONE",   done);
        sym_override("NCARDS", ncards);
        asm_pass2(0);
        if (g_errors) {
            fprintf(stderr, "gasm: %d error(s) in boot template\n", g_errors);
            return 1;
        }
        if (img_min != 0 || img_max > 40) {
            fprintf(stderr, "gasm: boot template must ORG at 0 and fit 40 "
                            "bytes (got 0x%04lX..0x%04lX)\n", img_min, img_max);
            return 1;
        }
        uint8_t boot[40];
        memset(boot, 0, sizeof(boot));
        memcpy(boot, image, (size_t)img_max);

        FILE *out = fopen(outpath, "w");
        if (!out) { fprintf(stderr, "gasm: cannot write '%s'\n", outpath); return 2; }
        uint16_t cols[80];
        for (int i = 0; i < 40; i++) {
            cols[2 * i]     = cap_hexcol((unsigned)(boot[i] >> 4));
            cols[2 * i + 1] = cap_hexcol((unsigned)(boot[i] & 0x0F));
        }
        cap_write_card(out, 1, cols);
        for (long c = 0; c < ncards; c++) {
            for (int j = 0; j < 80; j++) {
                long off = c * 80 + j;
                cols[j] = off < plen ? cap_colbin(prog_img[off]) : 0;
            }
            cap_write_card(out, (int)c + 2, cols);
        }
        fclose(out);
        printf("gasm: boot deck %s: boot card + %ld body cards, "
               "load+entry 0x%04lX (arm %s)\n",
               outpath, ncards, porg, outpath);
        return 0;
    }

    /* ---- One IPL boot card ----------------------------------------
     *
     * The machine reads exactly ONE card at CLEAR/LOAD/START: 80 columns,
     * nibble-packed by the channel into 40 bytes at 0x0000, and executed
     * there. So a boot card is a raw image, ORG 0, 40 bytes at most -- and
     * whatever it does next, including reading the rest of a deck, it does
     * itself. */
    {
        uint16_t origin_c = (uint16_t)img_min;
        uint16_t length_c = (uint16_t)(img_max - img_min);
        uint16_t cols[80];
        uint8_t  card[40];
        FILE *out;

        if (origin_c != 0) {
            fprintf(stderr, "gasm: --card: image must ORG at 0x0000 (the "
                            "IPL executes the card there); origin is "
                            "0x%04X\n", origin_c);
            return 1;
        }
        if (length_c > 40) {
            fprintf(stderr, "gasm: --card: %u bytes exceed the 40-byte "
                            "card (80 hex columns); drop --card and let the "
                            "loader deck carry it\n", length_c);
            return 1;
        }
        if (g_entry > 0)
            fprintf(stderr, "gasm: warning: --card ignores ENTRY 0x%04lX "
                            "(the IPL always enters at 0x0000)\n", g_entry);

        memset(card, 0, sizeof(card));
        memcpy(card, image, (size_t)length_c);

        out = fopen(outpath, "w");
        if (!out) { fprintf(stderr, "gasm: cannot write '%s'\n", outpath); return 2; }
        for (int i = 0; i < 40; i++) {
            cols[2 * i]     = cap_hexcol((unsigned)(card[i] >> 4));
            cols[2 * i + 1] = cap_hexcol((unsigned)(card[i] & 0x0F));
        }
        cap_write_card(out, 1, cols);
        fclose(out);

        /* Column 3 carries the row-8 punch that both readers use to spot a
         * bootstrap card. It falls out naturally when byte 1 has high nibble
         * 8 -- which every card opening `PER 0x80` (9E 80 ...) does. A card
         * that starts otherwise is still read as the loader, because a
         * one-card deck has nothing else it could be, but say so. */
        if (cols[2] != 0x0100)
            fprintf(stderr, "gasm: note: %s carries no row-8 loader marker in "
                            "column 3 (byte 1 is 0x%02X, not 0x8_). A one-card "
                            "deck is read as its own loader anyway.\n",
                            outpath, card[1]);

        printf("gasm: boot card %s: %u/40 bytes used (ge %s, or arm %s)\n",
               outpath, length_c, outpath, outpath);
    }

    return 0;
}

/* ---- Pass 1: build symbol table, compute addresses ---- */
static void asm_pass1(long org)
{
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
}

/* ---- Pass 2: emit bytes ---- */
static void asm_pass2(long org)
{
    long lc = org;
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
}
