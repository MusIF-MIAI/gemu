#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static ge_log_type active_log_types = -1; // ~(LOG_CONDS | LOG_STATES);

static const char *log_type_name(ge_log_type type)
{
    switch (type) {
        case LOG_ERR:
            return "error   ";
        case LOG_DEBUG:
            return "debug   ";
        case LOG_REGS:
            return "regs    ";
        case LOG_STATES:
            return "states  ";
        case LOG_CONDS:
            return "conds   ";
        case LOG_REGS_V:
            return "regs v  ";
        case LOG_FUTURE:
            return "future  ";
        case LOG_CYCLE:
            return "cycle   ";
        case LOG_CONSOLE:
            return "console ";
        case LOG_PERI:
            return "periph  ";
        case LOG_READER:
            return "reader  ";
        case LOG_CMDS:
            return "cmds    ";
        default:
            return "        ";
    }
}

void ge_log_set_active_types(ge_log_type types)
{
    active_log_types = types;
}

void ge_log_set_active_types_from_spec(const char *spec)
{
    /* Fixed-size token buffer; names are short, 64 bytes is ample. */
    char tok[64];
    ge_log_type mask = 0;
    const char *p, *end;

    if (spec == NULL || spec[0] == '\0')
        return;

    p = spec;
    while (*p != '\0') {
        /* Skip leading spaces/commas */
        while (*p == ',' || isspace((unsigned char)*p))
            p++;
        if (*p == '\0')
            break;

        /* Find end of token */
        end = p;
        while (*end != '\0' && *end != ',')
            end++;

        /* Trim trailing spaces within the token */
        const char *token_end = end;
        while (token_end > p && isspace((unsigned char)*(token_end - 1)))
            token_end--;

        /* Copy token safely */
        size_t len = (size_t)(token_end - p);
        if (len >= sizeof(tok))
            len = sizeof(tok) - 1;
        memcpy(tok, p, len);
        tok[len] = '\0';

        /* Map token to bitmask */
        if      (strcmp(tok, "all")     == 0)
            mask |= (ge_log_type)-1;
        else if (strcmp(tok, "none")    == 0)
            mask  = 0;
        else if (strcmp(tok, "err")     == 0)
            mask |= LOG_ERR;
        else if (strcmp(tok, "debug")   == 0)
            mask |= LOG_DEBUG;
        else if (strcmp(tok, "regs")    == 0)
            mask |= LOG_REGS;
        else if (strcmp(tok, "states")  == 0)
            mask |= LOG_STATES;
        else if (strcmp(tok, "conds")   == 0)
            mask |= LOG_CONDS;
        else if (strcmp(tok, "regs_v")  == 0)
            mask |= LOG_REGS_V;
        else if (strcmp(tok, "future")  == 0)
            mask |= LOG_FUTURE;
        else if (strcmp(tok, "cycle")   == 0)
            mask |= LOG_CYCLE;
        else if (strcmp(tok, "console") == 0)
            mask |= LOG_CONSOLE;
        else if (strcmp(tok, "peri")    == 0)
            mask |= LOG_PERI;
        else if (strcmp(tok, "reader")  == 0)
            mask |= LOG_READER;
        else if (strcmp(tok, "cmds")    == 0)
            mask |= LOG_CMDS;
        else {
            /* Unknown category: emit an error but do not abort. */
            ge_log(LOG_ERR, "ge_log_set_active_types_from_spec: unknown category '%s'\n", tok);
        }

        p = end;
    }

    ge_log_set_active_types(mask);
}

void ge_log(ge_log_type type, const char *format, ...)
{
    static char line[0x1000];
    va_list args;

    if (!(active_log_types & type))
        return;

    va_start (args, format);
    vsnprintf(line, sizeof(line), format, args);
    va_end (args);

    printf("  %-7s ]    %s", log_type_name(type), line);
}

uint8_t ge_log_enabled(ge_log_type type) {
    return !!(active_log_types & type);
}
