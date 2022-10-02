#ifndef LOG_H
#define LOG_H

#include <stdint.h>

typedef uint8_t ge_log_type;

enum ge_log_types {
    LOG_ERR    = 0x01,
    LOG_DEBUG  = 0x02,
    LOG_REGS   = 0x04,
    LOG_STATES = 0x08,
    LOG_CONDS  = 0x10,
};

void ge_log_set_active_types(ge_log_type types);
void ge_log(ge_log_type type, const char *format, ...);

#endif /* LOG_H */
