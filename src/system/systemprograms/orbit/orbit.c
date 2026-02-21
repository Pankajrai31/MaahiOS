/**
 * Orbit - MaahiOS Desktop Shell
 * 
 * Uses Log Executive for logging via liblog
 */

#include <stdint.h>
#include "../../libraries/liblog/liblog.h"

/*=============================================================================
 * Syscall Numbers
 *===========================================================================*/
#define SYS_YIELD           1
#define SYS_KLOG            240

static inline int syscall0(int num) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num) : "memory");
    return ret;
}

static inline int syscall3(int num, uint32_t a, uint32_t b, uint32_t c) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(a), "c"(b), "d"(c) : "memory");
    return ret;
}

static void yield(void) {
    syscall0(SYS_YIELD);
}

/* Fallback klog if Log Executive not available */
static void klog_fallback(int level, const char *tag, const char *msg) {
    syscall3(SYS_KLOG, level, (uint32_t)tag, (uint32_t)msg);
}

/*=============================================================================
 * Main Entry Point
 *===========================================================================*/

void orbit_main_c(void) {
    /* Try to connect to Log Executive */
    int log_ok = (ulog_init() == 0);
    
    if (log_ok) {
        ulog(LOG_INFO, "ORBIT", "========================================");
        ulog(LOG_INFO, "ORBIT", "     Welcome to Orbit!");
        ulog(LOG_INFO, "ORBIT", "  MaahiOS Desktop Shell v2.0");
        ulog(LOG_INFO, "ORBIT", "========================================");
        ulog(LOG_INFO, "ORBIT", "Connected to Log Executive");
        ulog(LOG_INFO, "ORBIT", "Desktop shell started successfully");
        ulog(LOG_INFO, "ORBIT", "UIManager skipped - running in minimal mode");
        ulog(LOG_INFO, "ORBIT", "Entering idle loop...");
    } else {
        /* Fallback to klog syscall */
        klog_fallback(LOG_INFO, "ORBIT", "========================================");
        klog_fallback(LOG_INFO, "ORBIT", "     Welcome to Orbit!");
        klog_fallback(LOG_INFO, "ORBIT", "  MaahiOS Desktop Shell v2.0");
        klog_fallback(LOG_INFO, "ORBIT", "========================================");
        klog_fallback(LOG_WARN, "ORBIT", "Log Executive not available, using klog");
        klog_fallback(LOG_INFO, "ORBIT", "Desktop shell started successfully");
        klog_fallback(LOG_INFO, "ORBIT", "Entering idle loop...");
    }
    
    /* Simple event loop - just yield */
    while(1) {
        yield();
    }
}
} exec_request_queue_t;

/* Log request payload */
typedef struct {
    uint8_t  level;
    char     tag[LOG_MAX_TAG_LEN];
    char     msg[LOG_MAX_MSG_LEN];
} log_entry_req_t;

/* Log hex request payload */
typedef struct {
    uint8_t  level;
    char     tag[LOG_MAX_TAG_LEN];
    char     msg[LOG_MAX_MSG_LEN];
    uint32_t value;
} log_hex_req_t;

/*=============================================================================
 * Syscall Wrappers
 *===========================================================================*/

static inline int syscall0(int num) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num) : "memory");
    return ret;
}

static inline int syscall1(int num, uint32_t a) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(a) : "memory");
    return ret;
}

static inline int syscall3(int num, uint32_t a, uint32_t b, uint32_t c) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(a), "c"(b), "d"(c) : "memory");
    return ret;
}

static void yield(void) {
    syscall0(SYS_YIELD);
}

static void* shm_attach(int shm_id) {
    return (void*)syscall1(SYS_SHM_ATTACH, shm_id);
}

static int cell_read(const char *name, void *buf, uint32_t size) {
    return syscall3(SYS_CELL_READ, (uint32_t)name, (uint32_t)buf, size);
}

/*=============================================================================
 * String Helpers
 *===========================================================================*/

static void str_copy(char *dest, const char *src, int max) {
    int i = 0;
    while (src && src[i] && i < max - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

/*=============================================================================
 * Spinlock Helpers
 *===========================================================================*/

static inline void spinlock_acquire(volatile uint32_t *lock) {
    while (__sync_lock_test_and_set(lock, 1)) {
        __asm__ volatile("pause");
    }
}

static inline void spinlock_release(volatile uint32_t *lock) {
    __sync_lock_release(lock);
}

/*=============================================================================
 * Log Executive Client (liblog inline)
 *===========================================================================*/

static exec_request_queue_t *g_log_req_queue = (void*)0;
static uint32_t g_log_msg_id = 1;

static int ulog_init(void) {
    /* Read Log Executive's request queue SHM ID from cell */
    int shm_id = -1;
    int result = cell_read("system.exec.log.req_shm", &shm_id, sizeof(int));
    if (result < 0 || shm_id < 0) {
        return -1;
    }
    
    g_log_req_queue = (exec_request_queue_t *)shm_attach(shm_id);
    if (!g_log_req_queue) {
        return -1;
    }
    
    return 0;
}

static void ulog(int level, const char *tag, const char *msg) {
    if (!g_log_req_queue) return;
    
    exec_request_t req;
    req.msg_id = g_log_msg_id++;
    req.opcode = LOG_OP_LOG;
    req.flags = 0;
    
    log_entry_req_t *payload = (log_entry_req_t *)req.payload;
    payload->level = (uint8_t)level;
    str_copy(payload->tag, tag, LOG_MAX_TAG_LEN);
    str_copy(payload->msg, msg, LOG_MAX_MSG_LEN);
    req.payload_size = sizeof(log_entry_req_t);
    
    /* Push to queue */
    spinlock_acquire(&g_log_req_queue->lock);
    if (g_log_req_queue->count < EXEC_REQUEST_QUEUE_SIZE) {
        g_log_req_queue->requests[g_log_req_queue->tail] = req;
        g_log_req_queue->tail = (g_log_req_queue->tail + 1) % EXEC_REQUEST_QUEUE_SIZE;
        g_log_req_queue->count++;
    }
    spinlock_release(&g_log_req_queue->lock);
}

static void ulog_hex(int level, const char *tag, const char *msg, uint32_t value) {
    if (!g_log_req_queue) return;
    
    exec_request_t req;
    req.msg_id = g_log_msg_id++;
    req.opcode = LOG_OP_LOG_HEX;
    req.flags = 0;
    
    log_hex_req_t *payload = (log_hex_req_t *)req.payload;
    payload->level = (uint8_t)level;
    str_copy(payload->tag, tag, LOG_MAX_TAG_LEN);
    str_copy(payload->msg, msg, LOG_MAX_MSG_LEN);
    payload->value = value;
    req.payload_size = sizeof(log_hex_req_t);
    
    /* Push to queue */
    spinlock_acquire(&g_log_req_queue->lock);
    if (g_log_req_queue->count < EXEC_REQUEST_QUEUE_SIZE) {
        g_log_req_queue->requests[g_log_req_queue->tail] = req;
        g_log_req_queue->tail = (g_log_req_queue->tail + 1) % EXEC_REQUEST_QUEUE_SIZE;
        g_log_req_queue->count++;
    }
    spinlock_release(&g_log_req_queue->lock);
}

/*=============================================================================
 * Fallback KLOG (if Log Executive not available)
 *===========================================================================*/
#define SYS_KLOG 240

static void klog_fallback(int level, const char *tag, const char *msg) {
    syscall3(SYS_KLOG, level, (uint32_t)tag, (uint32_t)msg);
}

/*=============================================================================
 * Main Entry Point
 *===========================================================================*/

void orbit_main_c(void) {
    /* Try to connect to Log Executive */
    int log_ok = (ulog_init() == 0);
    
    if (log_ok) {
        ulog(LOG_INFO, "ORBIT", "========================================");
        ulog(LOG_INFO, "ORBIT", "     Welcome to Orbit!");
        ulog(LOG_INFO, "ORBIT", "  MaahiOS Desktop Shell v2.0");
        ulog(LOG_INFO, "ORBIT", "========================================");
        ulog(LOG_INFO, "ORBIT", "Connected to Log Executive");
        ulog(LOG_INFO, "ORBIT", "Desktop shell started successfully");
        ulog(LOG_INFO, "ORBIT", "UIManager skipped - running in minimal mode");
        ulog(LOG_INFO, "ORBIT", "Entering idle loop...");
    } else {
        /* Fallback to klog syscall */
        klog_fallback(LOG_INFO, "ORBIT", "========================================");
        klog_fallback(LOG_INFO, "ORBIT", "     Welcome to Orbit!");
        klog_fallback(LOG_INFO, "ORBIT", "  MaahiOS Desktop Shell v2.0");
        klog_fallback(LOG_INFO, "ORBIT", "========================================");
        klog_fallback(LOG_WARN, "ORBIT", "Log Executive not available, using klog");
        klog_fallback(LOG_INFO, "ORBIT", "Desktop shell started successfully");
        klog_fallback(LOG_INFO, "ORBIT", "Entering idle loop...");
    }
    
    /* Simple event loop - just yield */
    while(1) {
        yield();
    }
}
