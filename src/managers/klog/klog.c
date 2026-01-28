/*
 * MaahiOS Kernel Logger Manager (klog)
 * 
 * SILENT buffering system:
 * - NO automatic serial output (buffer only)
 * - Dump to serial only when:
 *   1. User presses Ctrl+Alt+Shift
 *   2. Exception handler calls klog_dump()
 *   3. Manual klog_dump() call from debugger
 * 
 * Works from both Ring 0 (kernel) and Ring 3 (user processes)
 */

#include "klog.h"

/* Serial output helper (only used for dumps) */
static void serial_putc(char c) {
    while ((*(volatile unsigned char*)0x3FD & 0x20) == 0);
    *(volatile unsigned char*)0x3F8 = c;
}

static void serial_print(const char *str) {
    while (*str) {
        serial_putc(*str++);
    }
}

/* Ring buffer storage */
static klog_entry_t log_buffer[KLOG_BUFFER_SIZE];
static int log_head = 0;        /* Next write position */
static int log_count = 0;       /* Number of entries */

/* Level names for dump output */
static const char* level_names[] = {
    "FATAL", "ERROR", "WARN ", "INFO ", "DEBUG", "TRACE"
};

/* String copy helper */
static void klog_strcpy(char *dest, const char *src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

/**
 * Initialize the kernel logger manager
 */
void klog_manager_init(void) {
    log_head = 0;
    log_count = 0;
    
    /* Clear buffer */
    for (int i = 0; i < KLOG_BUFFER_SIZE; i++) {
        log_buffer[i].timestamp = 0;
        log_buffer[i].level = 0;
        log_buffer[i].pid = 0;
        log_buffer[i].tag[0] = '\0';
        log_buffer[i].msg[0] = '\0';
    }
    
    /* Silent init - no banner */
}

/**
 * Log a message (SILENT - buffer only, no serial output)
 */
void klog(int level, const char *tag, const char *msg) {
    /* Filter by compile-time level */
    if (level > KLOG_MIN_LEVEL) {
        return;
    }
    
    /* Store in ring buffer */
    klog_entry_t *entry = &log_buffer[log_head];
    entry->timestamp = 0;  /* TODO: Add timer ticks */
    entry->level = (unsigned char)level;
    entry->pid = 0;  /* TODO: Get current PID from scheduler */
    klog_strcpy(entry->tag, tag ? tag : "???", KLOG_MAX_TAG_LEN);
    klog_strcpy(entry->msg, msg ? msg : "", KLOG_MAX_MSG_LEN);
    
    /* Advance ring buffer (circular) */
    log_head = (log_head + 1) % KLOG_BUFFER_SIZE;
    if (log_count < KLOG_BUFFER_SIZE) {
        log_count++;
    }
    
    /* NO serial output - silent buffering only */
}

/**
 * Log with hex value
 */
void klog_hex(int level, const char *tag, const char *msg, unsigned int value) {
    /* Filter by compile-time level */
    if (level > KLOG_MIN_LEVEL) {
        return;
    }
    
    /* Build message with hex value appended */
    char full_msg[KLOG_MAX_MSG_LEN];
    int i = 0;
    
    /* Copy message */
    if (msg) {
        while (msg[i] && i < KLOG_MAX_MSG_LEN - 12) {
            full_msg[i] = msg[i];
            i++;
        }
    }
    
    /* Append hex value */
    const char hex[] = "0123456789ABCDEF";
    full_msg[i++] = ' ';
    full_msg[i++] = '0';
    full_msg[i++] = 'x';
    for (int j = 7; j >= 0 && i < KLOG_MAX_MSG_LEN - 1; j--) {
        full_msg[i++] = hex[(value >> (j * 4)) & 0xF];
    }
    full_msg[i] = '\0';
    
    /* Log the combined message */
    klog(level, tag, full_msg);
}

/**
 * Log with two hex values
 */
void klog_hex2(int level, const char *tag, const char *msg, 
               unsigned int val1, unsigned int val2) {
    if (level > KLOG_MIN_LEVEL) return;
    
    /* Build message with both hex values */
    char full_msg[KLOG_MAX_MSG_LEN];
    int i = 0;
    
    /* Copy message */
    if (msg) {
        while (msg[i] && i < KLOG_MAX_MSG_LEN - 25) {
            full_msg[i] = msg[i];
            i++;
        }
    }
    
    /* Append hex values */
    const char hex[] = "0123456789ABCDEF";
    full_msg[i++] = ' ';
    full_msg[i++] = '0';
    full_msg[i++] = 'x';
    for (int j = 7; j >= 0 && i < KLOG_MAX_MSG_LEN - 13; j--) {
        full_msg[i++] = hex[(val1 >> (j * 4)) & 0xF];
    }
    full_msg[i++] = ',';
    full_msg[i++] = ' ';
    full_msg[i++] = '0';
    full_msg[i++] = 'x';
    for (int j = 7; j >= 0 && i < KLOG_MAX_MSG_LEN - 1; j--) {
        full_msg[i++] = hex[(val2 >> (j * 4)) & 0xF];
    }
    full_msg[i] = '\0';
    
    klog(level, tag, full_msg);
}

/**
 * Dump entire ring buffer to serial output
 * Triggered by Ctrl+Alt+Shift or exception handlers
 */
void klog_dump(void) {
    serial_print("\n");
    serial_print("================================================================================\n");
    serial_print("                          KLOG BUFFER DUMP\n");
    serial_print("================================================================================\n");
    
    if (log_count == 0) {
        serial_print("  (no log entries)\n");
    } else {
        /* Find oldest entry */
        int start = (log_count < KLOG_BUFFER_SIZE) ? 0 : log_head;
        
        for (int i = 0; i < log_count; i++) {
            int idx = (start + i) % KLOG_BUFFER_SIZE;
            klog_entry_t *entry = &log_buffer[idx];
            
            /* Build line: [N] [LEVEL][TAG] message */
            char line[140];
            int p = 0;
            
            /* Entry number */
            line[p++] = '[';
            int n = i;
            if (n >= 10) {
                line[p++] = '0' + (n / 10);
                n = n % 10;
            }
            line[p++] = '0' + n;
            line[p++] = ']';
            line[p++] = ' ';
            
            /* Level */
            line[p++] = '[';
            const char *lvl = (entry->level <= 5) ? level_names[entry->level] : "?????";
            while (*lvl) line[p++] = *lvl++;
            line[p++] = ']';
            
            /* Tag */
            line[p++] = '[';
            const char *t = entry->tag;
            while (*t && p < 130) line[p++] = *t++;
            line[p++] = ']';
            line[p++] = ' ';
            
            /* Message */
            const char *m = entry->msg;
            while (*m && p < 138) line[p++] = *m++;
            line[p++] = '\n';
            line[p] = '\0';
            
            serial_print(line);
        }
    }
    
    serial_print("================================================================================\n");
    serial_print("                         END OF KLOG DUMP\n");
    serial_print("================================================================================\n\n");
}

/**
 * Get log buffer for inspection
 */
klog_entry_t* klog_get_buffer(int *count) {
    if (count) {
        *count = log_count;
    }
    /* Return pointer to oldest entry */
    int start = (log_count < KLOG_BUFFER_SIZE) ? 0 : log_head;
    return &log_buffer[start];
}
