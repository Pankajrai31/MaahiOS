/*
 * MaahiOS Kernel Logger Manager (klog)
 * 
 * Ring buffer-based logging system for both kernel and user mode.
 * - SILENT buffering (no live serial output)
 * - Dump on demand via Ctrl+Alt+Shift or on exception
 * - Works from both Ring 0 (kernel) and Ring 3 (user processes)
 * - 256 entry circular buffer (~25KB static)
 * 
 * Usage:
 *   KLOG_INFO("TAG", "message");
 *   KLOG_INFO_HEX("TAG", "value=", 0x1234);
 *   KLOG_DEBUG_HEX2("TAG", "a,b=", val_a, val_b);
 */

#ifndef KLOG_H
#define KLOG_H

/* ============================================
 * Log Severity Levels (lower = more severe)
 * ============================================ */
#define LOG_FATAL   0   /* System is unusable */
#define LOG_ERROR   1   /* Error conditions */
#define LOG_WARN    2   /* Warning conditions */
#define LOG_INFO    3   /* Informational */
#define LOG_DEBUG   4   /* Debug-level messages */
#define LOG_TRACE   5   /* Very verbose tracing */

/* Compile-time minimum level (messages above this are compiled out) */
#ifndef KLOG_MIN_LEVEL
#define KLOG_MIN_LEVEL LOG_DEBUG
#endif

/* ============================================
 * Configuration
 * ============================================ */
#define KLOG_MAX_TAG_LEN    12
#define KLOG_MAX_MSG_LEN    80
#define KLOG_BUFFER_SIZE    256

/* ============================================
 * Log Entry Structure
 * ============================================ */
typedef struct {
    unsigned int timestamp;          /* Timer ticks since boot */
    unsigned char level;             /* Severity level */
    unsigned char pid;               /* Process ID that logged */
    char tag[KLOG_MAX_TAG_LEN];      /* Component tag */
    char msg[KLOG_MAX_MSG_LEN];      /* Log message */
} klog_entry_t;

/* ============================================
 * Core API
 * ============================================ */

/** Initialize the kernel logger. Must be called before any logging. */
void klog_manager_init(void);

/** Log a formatted message (silent - buffer only, no serial output).
 *  Supports printf-style: %d, %u, %x, %X, %s, %c, %% */
void klog(int level, const char *tag, const char *fmt, ...);

/** Log with one hex value appended. */
void klog_hex(int level, const char *tag, const char *msg, unsigned int value);

/** Log with two hex values appended. */
void klog_hex2(int level, const char *tag, const char *msg, 
               unsigned int val1, unsigned int val2);

/** Dump entire ring buffer to serial (COM1). */
void klog_dump(void);

/** Get pointer to ring buffer for inspection. */
klog_entry_t* klog_get_buffer(int *count);

/**
 * klog_read_entries - Copy log entries to a user-supplied buffer
 * @dst:         Destination buffer (array of klog_entry_t)
 * @max_entries: Maximum entries to copy
 *
 * Copies entries in chronological order (oldest first).
 * Returns: Number of entries actually copied.
 */
int klog_read_entries(klog_entry_t *dst, int max_entries);

/* ============================================
 * User-space Log API (Ring 3 → kernel via syscall)
 * Messages are prefixed with [U] to distinguish from kernel logs.
 * ============================================ */

/** Log a message from user space (prefixed with [U]). */
void ulog(int level, const char *tag, const char *msg);

/** Log a message with hex value from user space (prefixed with [U]). */
void ulog_hex(int level, const char *tag, const char *msg, unsigned int value);

/* ============================================
 * Convenience Macros - Plain/Formatted Message
 * Support printf-style: KLOG_INFO("TAG", "val=%d", x)
 * ============================================ */
#define KLOG_FATAL(tag, ...)       klog(LOG_FATAL, tag, __VA_ARGS__)
#define KLOG_ERROR(tag, ...)       klog(LOG_ERROR, tag, __VA_ARGS__)
#define KLOG_WARN(tag, ...)        klog(LOG_WARN, tag, __VA_ARGS__)
#define KLOG_INFO(tag, ...)        klog(LOG_INFO, tag, __VA_ARGS__)
#define KLOG_DEBUG(tag, ...)       klog(LOG_DEBUG, tag, __VA_ARGS__)
#define KLOG_TRACE(tag, ...)       klog(LOG_TRACE, tag, __VA_ARGS__)

/* ============================================
 * Convenience Macros - One Hex Value
 * ============================================ */
#define KLOG_FATAL_HEX(tag, msg, v)  klog_hex(LOG_FATAL, tag, msg, v)
#define KLOG_ERROR_HEX(tag, msg, v)  klog_hex(LOG_ERROR, tag, msg, v)
#define KLOG_WARN_HEX(tag, msg, v)   klog_hex(LOG_WARN, tag, msg, v)
#define KLOG_INFO_HEX(tag, msg, v)   klog_hex(LOG_INFO, tag, msg, v)
#define KLOG_DEBUG_HEX(tag, msg, v)  klog_hex(LOG_DEBUG, tag, msg, v)
#define KLOG_TRACE_HEX(tag, msg, v)  klog_hex(LOG_TRACE, tag, msg, v)

/* ============================================
 * Convenience Macros - Two Hex Values
 * ============================================ */
#define KLOG_FATAL_HEX2(tag, msg, v1, v2)  klog_hex2(LOG_FATAL, tag, msg, v1, v2)
#define KLOG_ERROR_HEX2(tag, msg, v1, v2)  klog_hex2(LOG_ERROR, tag, msg, v1, v2)
#define KLOG_WARN_HEX2(tag, msg, v1, v2)   klog_hex2(LOG_WARN, tag, msg, v1, v2)
#define KLOG_INFO_HEX2(tag, msg, v1, v2)   klog_hex2(LOG_INFO, tag, msg, v1, v2)
#define KLOG_DEBUG_HEX2(tag, msg, v1, v2)  klog_hex2(LOG_DEBUG, tag, msg, v1, v2)
#define KLOG_TRACE_HEX2(tag, msg, v1, v2)  klog_hex2(LOG_TRACE, tag, msg, v1, v2)

#endif /* KLOG_H */
