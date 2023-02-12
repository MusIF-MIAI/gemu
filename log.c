#include "log.h"

#include <stdarg.h>
#include <stdio.h>

static ge_log_type active_log_types = ~(LOG_CONDS | LOG_STATES);

static const char *log_type_name(ge_log_type type)
{
    switch (type) {
        case LOG_ERR:    return "error  ";
        case LOG_DEBUG:  return "debug  ";
        case LOG_REGS:   return "regs   ";
        case LOG_STATES: return "states ";
        case LOG_CONDS:  return "conds  ";
        default:         return "       ";
    }
}

void ge_log_set_active_types(ge_log_type types)
{
    active_log_types = types;
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

    printf("  %-6s  ]    %s", log_type_name(type), line);
}
