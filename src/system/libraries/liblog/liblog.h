/**
 * MaahiOS User-Space Logging Library (liblog)
 * 
 * User-space apps use this library to log via Log Executive.
 * Auto-initializes on first call. Falls back to direct klog
 * syscall if Log Executive is not yet running.
 * 
 * Usage:
 *   #include "liblog.h"
 *   
 *   liblog(LOG_INFO, "MYAPP", "Hello world");     // just call it
 *   liblog_hex(LOG_INFO, "MYAPP", "Value:", 0x1234);
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#ifndef LIBLOG_H
#define LIBLOG_H

#include <stdint.h>

/* Log levels (same as klog) */
#define LOG_FATAL   0
#define LOG_ERROR   1
#define LOG_WARN    2
#define LOG_INFO    3
#define LOG_DEBUG   4
#define LOG_TRACE   5

/* Executive ID for Log Executive */
#define EXEC_ID_LOG             6

/* Function IDs for Log Executive (must match log_executive.h) */
#define LOG_FUNC_LOG            16  /* LOG_OP_LOG = EXEC_OP_CUSTOM_BASE + 0 */
#define LOG_FUNC_LOG_HEX        17  /* LOG_OP_LOG_HEX = EXEC_OP_CUSTOM_BASE + 1 */

/* Configuration */
#define LIBLOG_QUEUE_SIZE        32
#define LIBLOG_MSG_MAX_PAYLOAD   256
#define LIBLOG_MAX_TAG_LEN       16   /* Must match LOG_MAX_TAG_LEN */
#define LIBLOG_MAX_MSG_LEN       128  /* Must match LOG_MAX_MSG_LEN */

/*=============================================================================
 * PUBLIC API
 *===========================================================================*/

/**
 * liblog_init - Explicitly initialize liblog (optional)
 * Auto-called on first use of liblog()/liblog_hex().
 * Returns: 0 on success, -1 if executive not ready
 */
int liblog_init(void);

/**
 * liblog_ready - Check if liblog is initialized
 * Returns: 1 if ready, 0 if not
 */
int liblog_ready(void);

/**
 * liblog - Log a message via Log Executive
 * @level: Log level (LOG_INFO, LOG_ERROR, etc.)
 * @tag: Short tag identifying the component
 * @msg: Message to log
 */
void liblog(int level, const char *tag, const char *msg);

/**
 * liblog_hex - Log a message with hex value via Log Executive
 * @level: Log level
 * @tag: Short tag
 * @msg: Message prefix
 * @value: Hex value to append
 */
void liblog_hex(int level, const char *tag, const char *msg, uint32_t value);

#endif /* LIBLOG_H */
