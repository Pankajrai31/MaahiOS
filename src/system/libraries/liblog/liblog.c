/**
 * MaahiOS User-Space Logging Library (liblog) - Implementation
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#include "liblog.h"

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
 * SYSCALL WRAPPERS
 *===========================================================================*/

#define SYS_SHM_ATTACH  49
#define SYS_CELL_READ   65   /* Fixed: was 69, should be 65 per syscall_numbers.h */
#define SYS_GETPID      100

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

static inline int syscall0(int num) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num) : "memory");
    return ret;
}

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
 * PUBLIC API IMPLEMENTATION
 *===========================================================================*/

int liblog_init(void) {
    if (g_initialized) return 0;
    
    /* Get our PID */
    g_my_pid = (uint32_t)syscall0(SYS_GETPID);
    
    /* Read Log Executive's queue SHM ID from cell */
    int shm_id = -1;
    int result = syscall3(SYS_CELL_READ, 
                          (uint32_t)"system.exec.log.req_shm", 
                          (uint32_t)&shm_id, sizeof(int));
    if (result < 0 || shm_id < 0) {
        return -1;
    }
    
    /* Attach to queue SHM */
    g_queue = (liblog_queue_t *)syscall1(SYS_SHM_ATTACH, shm_id);
    if (!g_queue || (uint32_t)g_queue == 0xFFFFFFFF) {
        g_queue = (void*)0;
        return -1;
    }
    
    g_initialized = 1;
    return 0;
}

int liblog_ready(void) {
    return g_initialized;
}

void liblog(int level, const char *tag, const char *msg) {
    if (!g_queue) return;
    
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
}

void liblog_hex(int level, const char *tag, const char *msg, uint32_t value) {
    if (!g_queue) return;
    
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
}
