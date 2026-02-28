/**
 * MaahiOS User-Space Logging Library (liblog) - Implementation
 * 
 * Description:
 *   User-space library for logging via Log Executive.
 *   Auto-initializes on first call (discovers SHM, attaches).
 *   Falls back to direct SYS_KLOG kernel syscall if Log Executive
 *   is not yet running.
 * 
 * Usage:
 *   liblog(LOG_INFO, "MYAPP", "Hello");       // just call it
 *   liblog_hex(LOG_INFO, "MYAPP", "val:", 42); // just call it
 *   No init() needed — handled automatically.
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#include "liblog.h"
#include "../core/syscall_helpers.h"

/*=============================================================================
 * INTERNAL TYPES (must match executive_common.h request structure)
 *===========================================================================*/

typedef struct {
    uint32_t msg_id;
    uint32_t sender_pid;
    uint32_t exec_id;
    uint32_t func_id;
    uint32_t flags;
    uint32_t payload_size;
    uint8_t  payload[LIBLOG_MSG_MAX_PAYLOAD];
} liblog_request_t;

typedef struct {
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint32_t count;
    volatile uint32_t lock;
    uint32_t reserved[4];
    liblog_request_t requests[LIBLOG_QUEUE_SIZE];
} liblog_queue_t;

/* Log entry payload */
typedef struct {
    uint8_t level;
    char tag[LIBLOG_MAX_TAG_LEN];
    char msg[LIBLOG_MAX_MSG_LEN];
} liblog_entry_t;

/* Log hex payload */
typedef struct {
    uint8_t level;
    char tag[LIBLOG_MAX_TAG_LEN];
    char msg[LIBLOG_MAX_MSG_LEN];
    uint32_t value;
} liblog_hex_entry_t;

/*=============================================================================
 * LIBRARY STATE
 *===========================================================================*/

static liblog_queue_t *g_queue = (void*)0;
static uint32_t g_msg_id = 1;
static int g_initialized = 0;
static uint32_t g_my_pid = 0;

/*=============================================================================
 * HELPER FUNCTIONS
 *===========================================================================*/

static void str_copy(char *dest, const char *src, int max) {
    int i = 0;
    while (src && src[i] && i < max - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

static void spinlock_acquire(volatile uint32_t *lock) {
    while (__sync_lock_test_and_set(lock, 1)) {
        __asm__ volatile("pause");
    }
}

static void spinlock_release(volatile uint32_t *lock) {
    __sync_lock_release(lock);
}

/*=============================================================================
 * INTERNAL: AUTO-INIT (lazy, called on first use)
 *===========================================================================*/

/**
 * _liblog_try_init - Try to connect to Log Executive's SHM queue.
 * Called automatically on first use. Non-fatal if executive not ready.
 * Returns: 0 on success, -1 if executive not available yet.
 */
static int _liblog_try_init(void) {
    if (g_initialized) return 0;
    
    /* Get our PID (only once) and seed msg_id to avoid collisions.
     * Multiple processes share the same executive response queue,
     * so msg_ids must be unique per process. */
    if (g_my_pid == 0) {
        g_my_pid = (uint32_t)syscall0(SYS_GETPID);
        g_msg_id = (g_my_pid << 16) | 1;
    }
    
    /* Read Log Executive's queue SHM ID from cell registry */
    int shm_id = -1;
    int result = syscall3(SYS_CELL_READ, 
                          (uint32_t)"system.exec.log.req_shm", 
                          (uint32_t)&shm_id, sizeof(int));
    if (result < 0 || shm_id < 0) {
        return -1;  /* Executive not registered yet */
    }
    
    /* Attach to queue SHM (virt_addr=0 lets kernel pick address) */
    g_queue = (liblog_queue_t *)syscall2(SYS_SHM_ATTACH, shm_id, 0);
    if (!g_queue || (uint32_t)g_queue == 0xFFFFFFFF) {
        g_queue = (void*)0;
        return -1;
    }
    
    g_initialized = 1;
    return 0;
}

/*=============================================================================
 * INTERNAL: DIRECT KLOG FALLBACK
 * Used when Log Executive is not yet running.
 *===========================================================================*/

static void _liblog_direct_klog(int level, const char *tag, const char *msg) {
    syscall3(SYS_KLOG, level, (int)tag, (int)msg);
}

static void _liblog_direct_klog_hex(int level, const char *tag, const char *msg, uint32_t value) {
    syscall4(SYS_KLOG_HEX, level, (int)tag, (int)msg, (int)value);
}

/*=============================================================================
 * PUBLIC API IMPLEMENTATION
 *===========================================================================*/

int liblog_init(void) {
    return _liblog_try_init();
}

int liblog_ready(void) {
    return g_initialized;
}

void liblog(int level, const char *tag, const char *msg) {
    /* Auto-init on first call */
    if (!g_initialized) {
        _liblog_try_init();
    }
    
    /* If connected to Log Executive, use SHM queue */
    if (g_queue) {
        spinlock_acquire(&g_queue->lock);
        
        if (g_queue->count < LIBLOG_QUEUE_SIZE) {
            liblog_request_t *req = &g_queue->requests[g_queue->tail];
            req->msg_id = g_msg_id++;
            req->sender_pid = g_my_pid;
            req->exec_id = EXEC_ID_LOG;
            req->func_id = LOG_FUNC_LOG;
            req->flags = 0;
            
            liblog_entry_t *entry = (liblog_entry_t *)req->payload;
            entry->level = (uint8_t)level;
            str_copy(entry->tag, tag, LIBLOG_MAX_TAG_LEN);
            str_copy(entry->msg, msg, LIBLOG_MAX_MSG_LEN);
            req->payload_size = sizeof(liblog_entry_t);
            
            g_queue->tail = (g_queue->tail + 1) % LIBLOG_QUEUE_SIZE;
            g_queue->count++;
        }
        
        spinlock_release(&g_queue->lock);
        return;
    }
    
    /* Fallback: Log Executive not ready, use direct kernel klog */
    _liblog_direct_klog(level, tag, msg);
}

void liblog_hex(int level, const char *tag, const char *msg, uint32_t value) {
    /* Auto-init on first call */
    if (!g_initialized) {
        _liblog_try_init();
    }
    
    /* If connected to Log Executive, use SHM queue */
    if (g_queue) {
        spinlock_acquire(&g_queue->lock);
        
        if (g_queue->count < LIBLOG_QUEUE_SIZE) {
            liblog_request_t *req = &g_queue->requests[g_queue->tail];
            req->msg_id = g_msg_id++;
            req->sender_pid = g_my_pid;
            req->exec_id = EXEC_ID_LOG;
            req->func_id = LOG_FUNC_LOG_HEX;
            req->flags = 0;
            
            liblog_hex_entry_t *entry = (liblog_hex_entry_t *)req->payload;
            entry->level = (uint8_t)level;
            str_copy(entry->tag, tag, LIBLOG_MAX_TAG_LEN);
            str_copy(entry->msg, msg, LIBLOG_MAX_MSG_LEN);
            entry->value = value;
            req->payload_size = sizeof(liblog_hex_entry_t);
            
            g_queue->tail = (g_queue->tail + 1) % LIBLOG_QUEUE_SIZE;
            g_queue->count++;
        }
        
        spinlock_release(&g_queue->lock);
        return;
    }
    
    /* Fallback: Log Executive not ready, use direct kernel klog */
    _liblog_direct_klog_hex(level, tag, msg, value);
}
