/*
 * MaahiOS Kernel Logger (klog)
 * Structured logging system with ring buffer for post-mortem analysis
 * Modeled after Linux kernel printk levels
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
 * Initialize the kernel logger
 * Must be called before any logging
 */
void klog_init(void);

/**
 * Log a message
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
 * Dump entire ring buffer to serial
 * Useful for post-mortem debugging
 */
void klog_dump(void);

/**
 * Get pointer to ring buffer for inspection
 * @param count  Output: number of entries in buffer
 * @return       Pointer to oldest entry
 */
klog_entry_t* klog_get_buffer(int *count);

/**
 * Enable/disable serial output
 * @param enabled  1 = output to serial, 0 = buffer only
 */
void klog_set_serial(int enabled);

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
