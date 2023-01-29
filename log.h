#ifndef LOG_H
#define LOG_H

#include <stdint.h>

typedef uint8_t ge_log_type;

/**
 * Log types from the emulator.
 *
 * Every log message from the emulator has associated a log type
 * such it can be filtered during execution to aid different kinds
 * of debugging.
 *
 * Log types can be combined by OR.
 */
enum ge_log_types {
    LOG_ERR    = 0x01, ///< Emulator unrecoverable condition
    LOG_DEBUG  = 0x02, ///< General detailed debug information
    LOG_REGS   = 0x04, ///< Register trace
    LOG_STATES = 0x08, ///< State trace
    LOG_CONDS  = 0x10, ///< MSL conditions trace
};

/**
 * Set active log types
 *
 * Specifies which set of logging messages should be displayed.
 *
 * @param types A set of log types
 */
void ge_log_set_active_types(ge_log_type types);

/**
 * Log message
 *
 * Specifies which set of logging messages should be displayed.
 *
 * @param type   The type of the message
 * @param format The printf-style format for the log message
 */
void ge_log(ge_log_type type, const char *format, ...);

#endif /* LOG_H */
