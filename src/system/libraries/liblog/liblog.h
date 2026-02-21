/**
 * MaahiOS User-Space Logging Library (liblog)
 * 
 * User-space apps use this library to log via Log Executive.
 * Log Executive writes to KLOG buffer with [U] prefix.
 * 
 * Usage:
 *   #include "liblog.h"
 *   
 *   liblog_init();  // Call once after Log Executive is running
 *   liblog(LOG_INFO, "MYAPP", "Hello world");
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

/* Function IDs for Log Executive */
#define LOG_FUNC_LOG            1   /* Log a message */
#define LOG_FUNC_LOG_HEX        2   /* Log a message with hex value */

/* Configuration */
#define LIBLOG_QUEUE_SIZE        32
#define LIBLOG_MSG_MAX_PAYLOAD   256
#define LIBLOG_MAX_TAG_LEN       12
#define LIBLOG_MAX_MSG_LEN       80

/*=============================================================================
 * PUBLIC API
 *===========================================================================*/

/**
 * liblog_init - Initialize liblog
 * Call after Log Executive is running.
 * Returns: 0 on success, -1 on failure
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
