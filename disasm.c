#include "disasm.h"
#include "opcodes.h"
#include <stdio.h>

/* Instruction format (operand shape), used only for text formatting here. */
typedef enum {
    D_JU,       /* PM: "mn addr"                */
    D_BRANCH,   /* PM: "mn mask, addr"          */
    D_REG,      /* PM: "mn N, addr"             */
    D_IMM,      /* PM: "mn K, addr"             */
    D_PER,      /* PM: "mn aux, addr"           */
    D_SS1,      /* SS: "mn len, A1, A2"         */
    D_SS2       /* SS: "mn l1, l2, A1, A2"      */
} dfmt_t;

struct dmnem { uint8_t op; const char *name; dfmt_t fmt; };

/* Opcode bytes come straight from opcodes.h (single source of truth). The P
 * format (HLT/NOP2 and the 0x02 group) and the 0x53 sense group are decoded by
 * second char / aux in ge_disasm_one() rather than from this table. */
static const struct dmnem DTAB[] = {
    { JC_OPCODE,   "JC",   D_BRANCH },
    { JCC_OPCODE,  "JCC",  D_BRANCH },
    { JU_OPCODE,   "JU",   D_JU     },
    { JRT_OPCODE,  "JRT",  D_PER    },
    { LR_OPCODE,   "LR",   D_REG    },
    { STR_OPCODE,  "STR",  D_REG    },
    { LA_OPCODE,   "LA",   D_REG    },
    { CMR_OPCODE,  "CMR",  D_REG    },
    { AMR_OPCODE,  "AMR",  D_REG    },
    { SMR_OPCODE,  "SMR",  D_REG    },
    { MVI_OPCODE,  "MVI",  D_IMM    },
    { NI_OPCODE,   "NI",   D_IMM    },
    { CMI_OPCODE,  "CMI",  D_IMM    },
    { CI_OPCODE,   "CI",   D_IMM    },
    { XI_OPCODE,   "XI",   D_IMM    },
    { TM_OPCODE,   "TM",   D_IMM    },
    { PER_OPCODE,  "PER",  D_PER    },
    { PERI_OPCODE, "PERI", D_PER    },
    { RDC_OPCODE,  "RDC",  D_PER    },
    { LPSR_OPCODE, "LPSR", D_PER    },
    { MVC_OPCODE,  "MVC",  D_SS1    },
    { NC_OPCODE,   "NC",   D_SS1    },
    { CMC_OPCODE,  "CMC",  D_SS1    },
    { OC_OPCODE,   "OC",   D_SS1    },
    { XC_OPCODE,   "XC",   D_SS1    },
    { TL_OPCODE,   "TL",   D_SS1    },
    { MVQ_OPCODE,  "MVQ",  D_SS1    },
    { CMQ_OPCODE,  "CMQ",  D_SS1    },
    { EDT_OPCODE,  "EDT",  D_SS1    },
    { SR_OPCODE,   "SR",   D_SS1    },
    { SL_OPCODE,   "SL",   D_SS1    },
    { PK_OPCODE,   "PK",   D_SS2    },
    { UPK_OPCODE,  "UPK",  D_SS2    },
    { PKS_OPCODE,  "PKS",  D_SS2    },
    { UPKS_OPCODE, "UPKS", D_SS2    },
    { MVP_OPCODE,  "MVP",  D_SS2    },
    { CMP_OPCODE,  "CMP",  D_SS2    },
    { AP_OPCODE,   "AP",   D_SS2    },
    { SP_OPCODE,   "SP",   D_SS2    },
    { MP_OPCODE,   "MP",   D_SS2    },
    { DP_OPCODE,   "DP",   D_SS2    },
    { AD_OPCODE,   "AD",   D_SS2    },
    { SD_OPCODE,   "SD",   D_SS2    },
    { AB_OPCODE,   "AB",   D_SS2    },
    { SB_OPCODE,   "SB",   D_SS2    },
};
#define NDTAB ((int)(sizeof(DTAB) / sizeof(DTAB[0])))

static const struct dmnem *dlookup(uint8_t op)
{
    for (int i = 0; i < NDTAB; i++)
        if (DTAB[i].op == op)
            return &DTAB[i];
    return NULL;
}

/* Address field: bit 15 set => modified field disp(N); else raw hex. */
static void fmt_addr(uint16_t field, char *buf, size_t n)
{
    if (field & 0x8000)
        snprintf(buf, n, "0x%03X(%d)", field & 0x0FFF, (field >> 12) & 7);
    else
        snprintf(buf, n, "0x%04X", field);
}

int ge_disasm_one(const uint8_t *mem, uint16_t addr, char *out, size_t outn)
{
    uint8_t op = mem[addr];
    char a1[24], a2[24];

    /* ---- P format (top two bits 00) ---- */
    if (op < 0x40) {
        uint8_t c2 = mem[(uint16_t)(addr + 1)];
        const char *mn = NULL;

        if (op == HLT_OPCODE)
            mn = "HLT";
        else if (op == NOP2_OPCODE)
            mn = "NOP2";
        else if (op == LON_OPCODE) {        /* the 0x02 sub-function group */
            if (c2 == ENS_2NDCHAR)  mn = "ENS";
            else if (c2 == INS_2NDCHAR)  mn = "INS";
            else if (c2 == LOFF_2NDCHAR) mn = "LOFF";
            else if (c2 == LON_2NDCHAR)  mn = "LON";
            else if (c2 == LOLL_2NDCHAR) mn = "LOLL";
        }

        if (!mn) {
            snprintf(out, outn, "DB     0x%02X", op);
            return 1;
        }
        snprintf(out, outn, "%s", mn);
        return 2;
    }

    /* ---- PM format (4 bytes) ---- */
    if (op < 0xC0) {
        uint8_t aux = mem[(uint16_t)(addr + 1)];
        uint16_t field = (uint16_t)((mem[(uint16_t)(addr + 2)] << 8) |
                                     mem[(uint16_t)(addr + 3)]);

        if (op == JS1_OPCODE) {             /* 0x53 sense group, by aux */
            const char *mn = (aux == JS1_2NDCHAR) ? "JS1"
                           : (aux == JS2_2NDCHAR) ? "JS2"
                           : (aux == JIE_2NDCHAR) ? "JIE" : NULL;
            if (!mn) { snprintf(out, outn, "DB     0x%02X", op); return 1; }
            fmt_addr(field, a1, sizeof a1);
            snprintf(out, outn, "%s %s", mn, a1);
            return 4;
        }

        const struct dmnem *m = dlookup(op);
        if (!m) { snprintf(out, outn, "DB     0x%02X", op); return 1; }
        fmt_addr(field, a1, sizeof a1);

        switch (m->fmt) {
        case D_JU:
            snprintf(out, outn, "%s %s", m->name, a1);
            return 4;
        case D_BRANCH:
            snprintf(out, outn, "%s 0x%02X, %s", m->name, aux & 0xF0, a1);
            return 4;
        case D_REG:
            snprintf(out, outn, "%s %d, %s", m->name, aux & 7, a1);
            return 4;
        case D_IMM:
        case D_PER:
            snprintf(out, outn, "%s 0x%02X, %s", m->name, aux, a1);
            return 4;
        default:
            snprintf(out, outn, "DB     0x%02X", op);
            return 1;
        }
    }

    /* ---- SS format (6 bytes) ---- */
    {
        const struct dmnem *m = dlookup(op);
        if (!m) { snprintf(out, outn, "DB     0x%02X", op); return 1; }
        uint8_t ll = mem[(uint16_t)(addr + 1)];
        uint16_t A1 = (uint16_t)((mem[(uint16_t)(addr + 2)] << 8) |
                                  mem[(uint16_t)(addr + 3)]);
        uint16_t A2 = (uint16_t)((mem[(uint16_t)(addr + 4)] << 8) |
                                  mem[(uint16_t)(addr + 5)]);
        fmt_addr(A1, a1, sizeof a1);
        fmt_addr(A2, a2, sizeof a2);
        if (m->fmt == D_SS1)
            snprintf(out, outn, "%s %d, %s, %s", m->name, ll + 1, a1, a2);
        else
            snprintf(out, outn, "%s %d, %d, %s, %s", m->name,
                     ((ll >> 4) & 0xf) + 1, (ll & 0xf) + 1, a1, a2);
        return 6;
    }
}

int ge_disasm_window(const uint8_t *mem, uint16_t pc,
                     int before, int after, char *out, size_t outn)
{
    char text[64];

    /* Resync: variable-length instructions can't be decoded backwards, so try
     * each start from pc-1 .. pc-16 and keep the furthest-back one whose forward
     * decode chain lands exactly on pc — that gives aligned preceding lines. */
    long start = pc;
    for (int lb = 1; lb <= 16; lb++) {
        long s = (long)pc - lb;
        if (s < 0)
            break;
        long a = s;
        while (a < pc) {
            int l = ge_disasm_one(mem, (uint16_t)a, text, sizeof text);
            a += (l > 0) ? l : 1;
        }
        if (a == pc)
            start = s;          /* aligned; prefer the longest such run */
    }

    /* Collect instruction start addresses from `start` to a bit past pc. */
    uint16_t addrs[80];
    int n = 0;
    long a = start;
    while (a <= (long)pc + 48 && n < (int)(sizeof addrs / sizeof addrs[0])) {
        addrs[n++] = (uint16_t)a;
        int l = ge_disasm_one(mem, (uint16_t)a, text, sizeof text);
        a += (l > 0) ? l : 1;
    }

    int cur = -1;
    for (int i = 0; i < n; i++)
        if (addrs[i] == pc) { cur = i; break; }
    if (cur < 0) { addrs[0] = pc; cur = 0; n = 1; }   /* fallback: pc only */

    int lo = cur - before; if (lo < 0) lo = 0;
    int hi = cur + after;  if (hi > n - 1) hi = n - 1;

    size_t used = 0;
    out[0] = '\0';
    for (int i = lo; i <= hi; i++) {
        uint16_t ad = addrs[i];
        int l = ge_disasm_one(mem, ad, text, sizeof text);
        if (l <= 0) l = 1;

        char hex[24];
        int hp = 0;
        for (int k = 0; k < l && hp < (int)sizeof(hex) - 3; k++)
            hp += snprintf(hex + hp, sizeof(hex) - (size_t)hp, "%02X ",
                           mem[(uint16_t)(ad + k)]);

        int wrote = snprintf(out + used, outn - used, "%c%04X: %-14s %s\n",
                             (i == cur) ? '>' : ' ', ad, hex, text);
        if (wrote < 0 || (size_t)wrote >= outn - used)
            break;
        used += (size_t)wrote;
    }

    return cur - lo;
}
