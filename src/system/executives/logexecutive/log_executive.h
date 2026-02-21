/**
 * MaahiOS Log Executive Header
 * 
 * Description:
 *   Log Executive manages user-space logging.
 *   Receives log requests via SHM queue, calls klog syscall to kernel.
 * 
 * Architecture:
 *   - Runs as Ring 3 process with HIGH priority
 *   - Receives requests via SHM request queue
 *   - Calls SYS_KLOG syscall to write to kernel log buffer
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#ifndef LOG_EXECUTIVE_H
#define LOG_EXECUTIVE_H

#include "../common/executive_common.h"

/*=============================================================================
 * EXECUTIVE IDENTIFICATION
 *===========================================================================*/

/* Executive ID is defined in executive_common.h as EXEC_ID_LOG = 6 */

/*=============================================================================
 * LOG BUFFER CONFIGURATION
 *===========================================================================*/

#define LOG_BUFFER_SIZE     256     /* Number of entries in ring buffer */
#define LOG_MAX_TAG_LEN     16      /* Max chars for tag */
#define LOG_MAX_MSG_LEN     128     /* Max chars for message */

/*=============================================================================
 * LOG LEVELS
 *===========================================================================*/

#define LOG_FATAL   0
#define LOG_ERROR   1
#define LOG_WARN    2
#define LOG_INFO    3
#define LOG_DEBUG   4
#define LOG_TRACE   5

#define LOG_MIN_LEVEL   LOG_DEBUG   /* Compile-time filter */

/*=============================================================================
 * LOG EXECUTIVE OPCODES (starting at EXEC_OP_CUSTOM_BASE)
 *===========================================================================*/

#define LOG_OP_LOG          (EXEC_OP_CUSTOM_BASE + 0)   /* Write log entry */
#define LOG_OP_LOG_HEX      (EXEC_OP_CUSTOM_BASE + 1)   /* Write log with hex value */
#define LOG_OP_GET_BUFFER   (EXEC_OP_CUSTOM_BASE + 2)   /* Get buffer SHM ID */
#define LOG_OP_CLEAR        (EXEC_OP_CUSTOM_BASE + 3)   /* Clear log buffer */

/*=============================================================================
 * REQUEST PAYLOAD STRUCTURES
 *===========================================================================*/

/* LOG_OP_LOG request payload */
typedef struct {
    uint8_t  level;                     /* Log level */
    char     tag[LOG_MAX_TAG_LEN];      /* Component tag */
    char     msg[LOG_MAX_MSG_LEN];      /* Log message */
} log_entry_req_t;

/* LOG_OP_LOG_HEX request payload */
typedef struct {
    uint8_t  level;                     /* Log level */
    char     tag[LOG_MAX_TAG_LEN];      /* Component tag */
    char     msg[LOG_MAX_MSG_LEN];      /* Log message */
    uint32_t value;                     /* Hex value to append */
} log_hex_req_t;

/*=============================================================================
 * LOG BUFFER ENTRY
 *===========================================================================*/

typedef struct {
    uint32_t timestamp;                 /* Timestamp (seconds of day) */
    uint8_t  level;                     /* Log level */
    uint8_t  reserved1;
    uint16_t pid;                       /* Process ID that logged */
    char     tag[LOG_MAX_TAG_LEN];      /* Component tag */
    char     msg[LOG_MAX_MSG_LEN];      /* Log message */
} log_entry_t;

/*=============================================================================
 * LOG BUFFER (SHM shared between Log Executive and readers)
 *===========================================================================*/

typedef struct {
    volatile uint32_t lock;             /* Spinlock for concurrent access */
    uint32_t head;                      /* Next write position */
    uint32_t count;                     /* Total entries (up to LOG_BUFFER_SIZE) */
    int32_t  shm_id;                    /* SHM ID for this buffer */
    log_entry_t entries[LOG_BUFFER_SIZE];
} log_buffer_t;

/*=============================================================================
 * LOG EXECUTIVE MAIN
 *===========================================================================*/

/**
 * exe_log_main - Log Executive entry point
 * 
 * Called after Ring 3 setup. Initializes and enters main processing loop.
 */
void exe_log_main(void);

#endif /* LOG_EXECUTIVE_H */
