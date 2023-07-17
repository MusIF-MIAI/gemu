#ifndef LOG_H
#define LOG_H

#include <stdint.h>

typedef int ge_log_type;

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
    LOG_ERR     = 0x001, ///< Emulator unrecoverable condition
    LOG_DEBUG   = 0x002, ///< General detailed debug information
    LOG_REGS    = 0x004, ///< Register trace per cycle
    LOG_STATES  = 0x008, ///< State trace
    LOG_CONDS   = 0x010, ///< MSL conditions trace
    LOG_REGS_V  = 0x020, ///< Register trace per pulse
    LOG_FUTURE  = 0x040, ///< Future state network debug
    LOG_CYCLE   = 0x080, ///< Cycle attribution debug
    LOG_CONSOLE = 0x100, ///< Console interactions
    LOG_PERI    = 0x200, ///< Peripherals IO
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

/**
 * Check if a log type is enabled
 *
 * @param type The type to check
 * @returns    true if the logtype mask is enabled
 */
uint8_t ge_log_enabled(ge_log_type type);

#endif /* LOG_H */
