/*
 * MaahiOS Kernel Logger Manager (klog)
 * 
 * Ring buffer-based logging system for both kernel and user mode
 * - NO live serial output (silent buffering only)
 * - Dump on demand via Ctrl+Alt+Shift or on exception
 * - Works from both Ring 0 (kernel) and Ring 3 (user processes)
 * - 64 entry circular buffer survives until manual dump
 */

#ifndef KLOG_H
#define KLOG_H

/* Log severity levels (lower = more severe) */
#define LOG_FATAL   0   /* System is unusable */
#define LOG_ERROR   1   /* Error conditions */
#define LOG_WARN    2   /* Warning conditions */
#define LOG_INFO    3   /* Informational */
#define LOG_DEBUG   4   /* Debug-level messages */
#define LOG_TRACE   5   /* Very verbose tracing */

/* Compile-time minimum level (messages below this are compiled out) */
#ifndef KLOG_MIN_LEVEL
#define KLOG_MIN_LEVEL LOG_DEBUG
#endif

/* Maximum log entry size */
#define KLOG_MAX_TAG_LEN    12
#define KLOG_MAX_MSG_LEN    80
#define KLOG_BUFFER_SIZE    64   /* Ring buffer entries */

/* Log entry structure */
typedef struct {
    unsigned int timestamp;          /* Timer ticks since boot */
    unsigned char level;             /* Severity level */
    unsigned char pid;               /* Process ID that logged */
    char tag[KLOG_MAX_TAG_LEN];      /* Component tag */
    char msg[KLOG_MAX_MSG_LEN];      /* Log message */
} klog_entry_t;

/**
 * Initialize the kernel logger manager
 * Must be called before any logging
 */
void klog_manager_init(void);

/**
 * Log a message (silent - buffer only, no serial output)
 * @param level  Severity level (LOG_FATAL to LOG_TRACE)
 * @param tag    Component identifier (e.g., "KERNEL", "SCHED", "UIMGR")
 * @param msg    Log message
 */
void klog(int level, const char *tag, const char *msg);

/**
 * Log with hex value appended
 * @param level  Severity level
 * @param tag    Component identifier
 * @param msg    Log message prefix
 * @param value  Hex value to append
 */
void klog_hex(int level, const char *tag, const char *msg, unsigned int value);

/**
 * Log with two hex values
 */
void klog_hex2(int level, const char *tag, const char *msg, 
               unsigned int val1, unsigned int val2);

/**
 * Dump entire ring buffer to serial output
 * Triggered by Ctrl+Alt+Shift or exception handlers
 */
void klog_dump(void);

/**
 * Get pointer to ring buffer for inspection
 * @param count  Output: number of entries in buffer
 * @return       Pointer to oldest entry
 */
klog_entry_t* klog_get_buffer(int *count);

/**
 * Convenience macros for common logging patterns
 */
#define KLOG_FATAL(tag, msg)       klog(LOG_FATAL, tag, msg)
#define KLOG_ERROR(tag, msg)       klog(LOG_ERROR, tag, msg)
#define KLOG_WARN(tag, msg)        klog(LOG_WARN, tag, msg)
#define KLOG_INFO(tag, msg)        klog(LOG_INFO, tag, msg)
#define KLOG_DEBUG(tag, msg)       klog(LOG_DEBUG, tag, msg)

#define KLOG_FATAL_HEX(tag, msg, v)  klog_hex(LOG_FATAL, tag, msg, v)
#define KLOG_ERROR_HEX(tag, msg, v)  klog_hex(LOG_ERROR, tag, msg, v)
#define KLOG_WARN_HEX(tag, msg, v)   klog_hex(LOG_WARN, tag, msg, v)
#define KLOG_INFO_HEX(tag, msg, v)   klog_hex(LOG_INFO, tag, msg, v)
#define KLOG_DEBUG_HEX(tag, msg, v)  klog_hex(LOG_DEBUG, tag, msg, v)

#endif /* KLOG_H */
