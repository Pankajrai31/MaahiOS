/*
 * MaahiOS Kernel Logger (klog)
 * 
 * Provides centralized logging with:
 * - Ring buffer (64 entries) for post-mortem crash analysis
 * - Formatted output to serial (for developers)
 * - Can dump logs to screen (for end users via error dialog)
 */

#include "klog.h"

/* Use existing kernel serial_print function */
extern void serial_print(const char *str);

/* Ring buffer storage */
static klog_entry_t log_buffer[KLOG_BUFFER_SIZE];
static int log_head = 0;        /* Next write position */
static int log_count = 0;       /* Number of entries */
static int serial_enabled = 1;  /* Output to serial by default */

/* Level names for output */
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

/* Convert hex to string */
static void hex_to_str(char *buf, unsigned int val) {
    const char hex[] = "0123456789ABCDEF";
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 8; i++) {
        buf[2 + i] = hex[(val >> ((7 - i) * 4)) & 0xF];
    }
    buf[10] = '\0';
}

/**
 * Initialize the kernel logger
 */
void klog_init(void) {
    log_head = 0;
    log_count = 0;
    serial_enabled = 1;
    
    /* Clear buffer */
    for (int i = 0; i < KLOG_BUFFER_SIZE; i++) {
        log_buffer[i].timestamp = 0;
        log_buffer[i].level = 0;
        log_buffer[i].pid = 0;
        log_buffer[i].tag[0] = '\0';
        log_buffer[i].msg[0] = '\0';
    }
    
    /* Print startup banner */
    serial_print("\n========================================\n");
    serial_print("    MaahiOS Kernel Logger Initialized\n");
    serial_print("========================================\n\n");
}

/**
 * Log a message
 */
void klog(int level, const char *tag, const char *msg) {
    /* Filter by compile-time level */
    if (level > KLOG_MIN_LEVEL) {
        return;
    }
    
    /* Store in ring buffer */
    klog_entry_t *entry = &log_buffer[log_head];
    entry->timestamp = 0;
    entry->level = (unsigned char)level;
    entry->pid = 0;
    klog_strcpy(entry->tag, tag ? tag : "???", KLOG_MAX_TAG_LEN);
    klog_strcpy(entry->msg, msg ? msg : "", KLOG_MAX_MSG_LEN);
    
    /* Advance ring buffer */
    log_head = (log_head + 1) % KLOG_BUFFER_SIZE;
    if (log_count < KLOG_BUFFER_SIZE) {
        log_count++;
    }
    
    /* Output to serial if enabled */
    if (serial_enabled) {
        /* Build formatted string: [LEVEL][TAG] message\n */
        char out[128];
        int i = 0;
        
        out[i++] = '[';
        const char *lvl = (level >= 0 && level <= 5) ? level_names[level] : "?????";
        while (*lvl && i < 120) out[i++] = *lvl++;
        out[i++] = ']';
        out[i++] = '[';
        const char *t = tag ? tag : "???";
        while (*t && i < 120) out[i++] = *t++;
        out[i++] = ']';
        out[i++] = ' ';
        const char *m = msg ? msg : "";
        while (*m && i < 120) out[i++] = *m++;
        out[i++] = '\n';
        out[i] = '\0';
        
        serial_print(out);
    }
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
    char hex1[12], hex2[12];
    int i = 0;
    
    hex_to_str(hex1, val1);
    hex_to_str(hex2, val2);
    
    /* Copy message */
    if (msg) {
        while (msg[i] && i < KLOG_MAX_MSG_LEN - 30) {
            full_msg[i] = msg[i];
            i++;
        }
    }
    
    /* Append first hex */
    for (int j = 0; hex1[j] && i < KLOG_MAX_MSG_LEN - 15; j++) {
        full_msg[i++] = hex1[j];
    }
    full_msg[i++] = ',';
    full_msg[i++] = ' ';
    
    /* Append second hex */
    for (int j = 0; hex2[j] && i < KLOG_MAX_MSG_LEN - 1; j++) {
        full_msg[i++] = hex2[j];
    }
    full_msg[i] = '\0';
    
    klog(level, tag, full_msg);
}

/**
 * Dump entire log buffer (useful for crash analysis)
 */
void klog_dump(void) {
    char num_buf[12];
    
    serial_print("\n========== KLOG DUMP START ==========\n");
    
    if (log_count == 0) {
        serial_print("(empty)\n");
    } else {
        /* Find oldest entry */
        int start = (log_count < KLOG_BUFFER_SIZE) ? 0 : log_head;
        
        for (int i = 0; i < log_count; i++) {
            int idx = (start + i) % KLOG_BUFFER_SIZE;
            klog_entry_t *entry = &log_buffer[idx];
            
            /* Build line: [N] [LEVEL][TAG] message */
            char line[140];
            int p = 0;
            
            line[p++] = '[';
            /* Convert i to decimal */
            int n = i, digits = 0;
            char tmp[10];
            if (n == 0) { tmp[digits++] = '0'; }
            while (n > 0) { tmp[digits++] = '0' + (n % 10); n /= 10; }
            while (--digits >= 0) line[p++] = tmp[digits];
            line[p++] = ']';
            line[p++] = ' ';
            
            line[p++] = '[';
            const char *lvl = (entry->level <= 5) ? level_names[entry->level] : "?????";
            while (*lvl) line[p++] = *lvl++;
            line[p++] = ']';
            
            line[p++] = '[';
            const char *t = entry->tag;
            while (*t) line[p++] = *t++;
            line[p++] = ']';
            line[p++] = ' ';
            
            const char *m = entry->msg;
            while (*m && p < 135) line[p++] = *m++;
            line[p++] = '\n';
            line[p] = '\0';
            
            serial_print(line);
        }
    }
    
    serial_print("=========== KLOG DUMP END ===========\n\n");
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

/**
 * Enable/disable serial output
 */
void klog_set_serial(int enabled) {
    serial_enabled = enabled;
}
