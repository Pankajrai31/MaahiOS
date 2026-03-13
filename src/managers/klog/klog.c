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
#include <stdarg.h>
#include "../../system/libraries/shared/io.h"

/* Serial port (COM1) constants */
#define COM1_DATA    0x3F8
#define COM1_IER     0x3F9
#define COM1_FIFO    0x3FA
#define COM1_LCR     0x3FB
#define COM1_MCR     0x3FC
#define COM1_LSR     0x3FD

/* Serial output helper */
static void serial_putc(char c) {
    while ((inb(COM1_LSR) & 0x20) == 0);  /* Wait for transmit buffer empty */
    outb(COM1_DATA, c);
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

/* ============================================
 * Mini vsnprintf for kernel (no libc)
 * Supports: %d, %u, %x, %X, %s, %c, %%, %08X
 * ============================================ */
static int klog_vsnprintf(char *buf, int size, const char *fmt, va_list ap) {
    int pos = 0;
    const char hex_lower[] = "0123456789abcdef";
    const char hex_upper[] = "0123456789ABCDEF";
    
    while (*fmt && pos < size - 1) {
        if (*fmt != '%') {
            buf[pos++] = *fmt++;
            continue;
        }
        fmt++;  /* skip '%' */
        
        /* Parse flags and width */
        char pad_char = ' ';
        int width = 0;
        
        if (*fmt == '0') {
            pad_char = '0';
            fmt++;
        }
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }
        
        switch (*fmt) {
            case 'd': {
                int val = va_arg(ap, int);
                char tmp[12];
                int len = 0;
                unsigned int uval;
                if (val < 0) {
                    buf[pos++] = '-';
                    uval = (unsigned int)(-val);
                } else {
                    uval = (unsigned int)val;
                }
                do {
                    tmp[len++] = '0' + (uval % 10);
                    uval /= 10;
                } while (uval > 0);
                while (len < width) { tmp[len++] = pad_char; }
                for (int i = len - 1; i >= 0 && pos < size - 1; i--) {
                    buf[pos++] = tmp[i];
                }
                break;
            }
            case 'u': {
                unsigned int val = va_arg(ap, unsigned int);
                char tmp[11];
                int len = 0;
                do {
                    tmp[len++] = '0' + (val % 10);
                    val /= 10;
                } while (val > 0);
                while (len < width) { tmp[len++] = pad_char; }
                for (int i = len - 1; i >= 0 && pos < size - 1; i--) {
                    buf[pos++] = tmp[i];
                }
                break;
            }
            case 'x': {
                unsigned int val = va_arg(ap, unsigned int);
                char tmp[9];
                int len = 0;
                do {
                    tmp[len++] = hex_lower[val & 0xF];
                    val >>= 4;
                } while (val > 0);
                while (len < width) { tmp[len++] = pad_char; }
                for (int i = len - 1; i >= 0 && pos < size - 1; i--) {
                    buf[pos++] = tmp[i];
                }
                break;
            }
            case 'X': {
                unsigned int val = va_arg(ap, unsigned int);
                char tmp[9];
                int len = 0;
                do {
                    tmp[len++] = hex_upper[val & 0xF];
                    val >>= 4;
                } while (val > 0);
                while (len < width) { tmp[len++] = pad_char; }
                for (int i = len - 1; i >= 0 && pos < size - 1; i--) {
                    buf[pos++] = tmp[i];
                }
                break;
            }
            case 's': {
                const char *s = va_arg(ap, const char *);
                if (!s) s = "(null)";
                while (*s && pos < size - 1) {
                    buf[pos++] = *s++;
                }
                break;
            }
            case 'c': {
                char c = (char)va_arg(ap, int);
                buf[pos++] = c;
                break;
            }
            case '%': {
                buf[pos++] = '%';
                break;
            }
            default:
                buf[pos++] = '%';
                if (pos < size - 1) buf[pos++] = *fmt;
                break;
        }
        fmt++;
    }
    buf[pos] = '\0';
    return pos;
}

/**
 * Initialize COM1 serial port (0x3F8) at 115200 baud, 8N1
 */
static void serial_init(void) {
    outb(COM1_IER,  0x00);  /* Disable interrupts */
    outb(COM1_LCR,  0x80);  /* Enable DLAB (set baud rate) */
    outb(COM1_DATA, 0x01);  /* Divisor lo: 115200 baud */
    outb(COM1_IER,  0x00);  /* Divisor hi */
    outb(COM1_LCR,  0x03);  /* 8 bits, no parity, 1 stop bit */
    outb(COM1_FIFO, 0xC7);  /* Enable FIFO, clear, 14-byte threshold */
    outb(COM1_MCR,  0x03);  /* DTR + RTS */
}

/**
 * Initialize the kernel logger manager
 */
void klog_manager_init(void) {
    log_head = 0;
    log_count = 0;
    
    /* Initialize serial port for live output */
    serial_init();
    
    /* Clear buffer */
    for (int i = 0; i < KLOG_BUFFER_SIZE; i++) {
        log_buffer[i].timestamp = 0;
        log_buffer[i].level = 0;
        log_buffer[i].pid = 0;
        log_buffer[i].tag[0] = '\0';
        log_buffer[i].msg[0] = '\0';
    }
}

/**
 * Store a pre-formatted message into the ring buffer
 * Also echoes immediately to serial for live monitoring
 */
static void klog_store(int level, const char *tag, const char *msg) {
    klog_entry_t *entry = &log_buffer[log_head];
    entry->timestamp = 0;  /* TODO: Add timer ticks */
    entry->level = (unsigned char)level;
    entry->pid = 0;  /* TODO: Get current PID from scheduler */
    klog_strcpy(entry->tag, tag ? tag : "???", KLOG_MAX_TAG_LEN);
    klog_strcpy(entry->msg, msg ? msg : "", KLOG_MAX_MSG_LEN);
    
    log_head = (log_head + 1) % KLOG_BUFFER_SIZE;
    if (log_count < KLOG_BUFFER_SIZE) {
        log_count++;
    }
    
    /* Live serial echo: [LEVEL][TAG] message */
    serial_putc('[');
    const char *lvl = (level <= 5) ? level_names[level] : "?????";
    serial_print(lvl);
    serial_putc(']');
    serial_putc('[');
    serial_print(tag ? tag : "???");
    serial_putc(']');
    serial_putc(' ');
    serial_print(msg ? msg : "");
    serial_putc('\n');
}

/**
 * Log a formatted message (SILENT - buffer only)
 * Supports printf-style format strings: %d, %u, %x, %X, %s, %c
 */
void klog(int level, const char *tag, const char *fmt, ...) {
    if (level > KLOG_MIN_LEVEL) {
        return;
    }
    
    char msg_buf[KLOG_MAX_MSG_LEN];
    va_list ap;
    va_start(ap, fmt);
    klog_vsnprintf(msg_buf, KLOG_MAX_MSG_LEN, fmt, ap);
    va_end(ap);
    
    klog_store(level, tag, msg_buf);
}

/**
 * Log with hex value appended
 */
void klog_hex(int level, const char *tag, const char *msg, unsigned int value) {
    if (level > KLOG_MIN_LEVEL) return;
    
    char full_msg[KLOG_MAX_MSG_LEN];
    int i = 0;
    
    if (msg) {
        while (msg[i] && i < KLOG_MAX_MSG_LEN - 12) {
            full_msg[i] = msg[i];
            i++;
        }
    }
    
    const char hex[] = "0123456789ABCDEF";
    full_msg[i++] = ' ';
    full_msg[i++] = '0';
    full_msg[i++] = 'x';
    for (int j = 7; j >= 0 && i < KLOG_MAX_MSG_LEN - 1; j--) {
        full_msg[i++] = hex[(value >> (j * 4)) & 0xF];
    }
    full_msg[i] = '\0';
    
    klog_store(level, tag, full_msg);
}

/**
 * Log with two hex values appended
 */
void klog_hex2(int level, const char *tag, const char *msg, 
               unsigned int val1, unsigned int val2) {
    if (level > KLOG_MIN_LEVEL) return;
    
    char full_msg[KLOG_MAX_MSG_LEN];
    int i = 0;
    
    if (msg) {
        while (msg[i] && i < KLOG_MAX_MSG_LEN - 25) {
            full_msg[i] = msg[i];
            i++;
        }
    }
    
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
    
    klog_store(level, tag, full_msg);
}

/**
 * Copy log entries to a user-supplied buffer (chronological order)
 * Returns number of entries copied.
 */
int klog_read_entries(klog_entry_t *dst, int max_entries) {
    if (!dst || max_entries <= 0) return 0;

    int to_copy = log_count;
    if (to_copy > max_entries) to_copy = max_entries;
    if (to_copy > KLOG_BUFFER_SIZE) to_copy = KLOG_BUFFER_SIZE;

    /* Find oldest entry */
    int start = (log_count < KLOG_BUFFER_SIZE) ? 0 : log_head;

    int i;
    for (i = 0; i < to_copy; i++) {
        int idx = (start + i) % KLOG_BUFFER_SIZE;
        /* Copy entry field-by-field */
        dst[i].timestamp = log_buffer[idx].timestamp;
        dst[i].level     = log_buffer[idx].level;
        dst[i].pid       = log_buffer[idx].pid;
        klog_strcpy(dst[i].tag, log_buffer[idx].tag, KLOG_MAX_TAG_LEN);
        klog_strcpy(dst[i].msg, log_buffer[idx].msg, KLOG_MAX_MSG_LEN);
    }

    return to_copy;
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
            if (n >= 100) {
                line[p++] = '0' + (n / 100);
                n = n % 100;
            }
            if (i >= 10) {
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

/**
 * User-space log (Ring 3 → kernel via syscall)
 * Prefixes message with [U] to distinguish from kernel [K] logs.
 */
void ulog(int level, const char *tag, const char *msg) {
    /* Build "[U] message" */
    char full_msg[KLOG_MAX_MSG_LEN];
    full_msg[0] = '[';
    full_msg[1] = 'U';
    full_msg[2] = ']';
    full_msg[3] = ' ';
    int i = 4;
    if (msg) {
        for (int j = 0; msg[j] && i < KLOG_MAX_MSG_LEN - 1; j++) {
            full_msg[i++] = msg[j];
        }
    }
    full_msg[i] = '\0';
    
    klog_store(level, tag, full_msg);
}

/**
 * User-space log with hex value (Ring 3 → kernel via syscall)
 * Prefixes message with [U] to distinguish from kernel [K] logs.
 */
void ulog_hex(int level, const char *tag, const char *msg, unsigned int value) {
    /* Build "[U] message 0xVALUE" */
    const char hex[] = "0123456789ABCDEF";
    char full_msg[KLOG_MAX_MSG_LEN];
    full_msg[0] = '[';
    full_msg[1] = 'U';
    full_msg[2] = ']';
    full_msg[3] = ' ';
    int i = 4;
    if (msg) {
        for (int j = 0; msg[j] && i < KLOG_MAX_MSG_LEN - 11; j++) {
            full_msg[i++] = msg[j];
        }
    }
    full_msg[i++] = '0';
    full_msg[i++] = 'x';
    for (int j = 7; j >= 0 && i < KLOG_MAX_MSG_LEN - 1; j--) {
        full_msg[i++] = hex[(value >> (j * 4)) & 0xF];
    }
    full_msg[i] = '\0';
    
    klog_store(level, tag, full_msg);
}
