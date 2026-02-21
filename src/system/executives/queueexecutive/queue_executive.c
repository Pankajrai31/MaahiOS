/**
 * MaahiOS Queue Executive Implementation
 * 
 * Description:
 *   Queue Executive manages message queues for IPC.
 *   Provides reliable message passing between processes.
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#include "queue_executive.h"
#include "../common/executive_queue.h"

/*=============================================================================
 * SYSCALL INTERFACE
 *===========================================================================*/

#define SYS_YIELD           1
#define SYS_SHM_CREATE      48
#define SYS_SHM_ATTACH      49
#define SYS_CELL_WRITE      64
#define SYS_KLOG            240
#define SYS_KLOG_HEX        241

/* Log levels */
#define LOG_INFO    3
#define LOG_WARN    2
#define LOG_ERROR   1

static inline int syscall5(int num, int a1, int a2, int a3, int a4, int a5) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(a1), "c"(a2), "d"(a3), "S"(a4), "D"(a5)
        : "memory"
    );
    return ret;
}

static inline int syscall4(int num, int a1, int a2, int a3, int a4) {
    return syscall5(num, a1, a2, a3, a4, 0);
}

static inline int syscall3(int num, int a1, int a2, int a3) {
    return syscall5(num, a1, a2, a3, 0, 0);
}

static inline int syscall1(int num, int a1) {
    return syscall5(num, a1, 0, 0, 0, 0);
}

static inline int syscall0(int num) {
    return syscall5(num, 0, 0, 0, 0, 0);
}

static inline void exe_yield(void) {
    syscall0(SYS_YIELD);
}

static inline int exe_shm_create(uint32_t size) {
    return syscall1(SYS_SHM_CREATE, (int)size);
}

static inline void* exe_shm_attach(int shm_id) {
    return (void*)syscall1(SYS_SHM_ATTACH, shm_id);
}

static inline int exe_cell_write(const char *name, const void *data, uint32_t size) {
    return syscall3(SYS_CELL_WRITE, (int)name, (int)data, (int)size);
}

/* Klog syscalls - output [U] since we're ring 3 calling via syscall */
static inline void exe_klog(int level, const char *tag, const char *msg) {
    syscall3(SYS_KLOG, level, (int)tag, (int)msg);
}

static inline void exe_klog_hex(int level, const char *tag, const char *msg, uint32_t value) {
    syscall4(SYS_KLOG_HEX, level, (int)tag, (int)msg, (int)value);
}

/*=============================================================================
 * INTERNAL: Queue Storage
 *===========================================================================*/

typedef struct {
    char name[QUEUE_NAME_MAX];
    int shm_id;
    uint32_t owner_pid;
    uint32_t max_messages;
    uint32_t message_size;
    uint32_t flags;
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint32_t count;
    volatile uint32_t lock;
    uint8_t *data;  /* Points to SHM message buffer */
} internal_queue_t;

static internal_queue_t g_queues[MAX_QUEUES];
static uint32_t g_queue_count = 0;
static uint32_t g_next_queue_id = 1;

/*=============================================================================
 * EXECUTIVE STATE
 *===========================================================================*/

static exec_control_block_t *g_ecb = NULL;
static exec_request_queue_t *g_req_queue = NULL;
static exec_response_queue_t *g_resp_queue = NULL;
static int g_req_queue_shm_id = -1;
static int g_resp_queue_shm_id = -1;

/*=============================================================================
 * QUEUE OPERATIONS
 *===========================================================================*/

static int exe_queue_create(const char *name, uint32_t max_msgs, uint32_t msg_size, 
                            uint32_t flags, uint32_t owner_pid) {
    if (g_queue_count >= MAX_QUEUES) {
        return EXEC_ERR_NO_MEMORY;
    }
    
    /* Allocate SHM for queue data */
    uint32_t data_size = max_msgs * msg_size;
    int shm_id = exe_shm_create(data_size);
    if (shm_id < 0) {
        return EXEC_ERR_NO_MEMORY;
    }
    
    void *data = exe_shm_attach(shm_id);
    if (!data) {
        return EXEC_ERR_NO_MEMORY;
    }
    
    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < MAX_QUEUES; i++) {
        if (g_queues[i].shm_id == 0) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        return EXEC_ERR_NO_MEMORY;
    }
    
    /* Initialize queue */
    exe_str_copy(g_queues[slot].name, name, QUEUE_NAME_MAX);
    g_queues[slot].shm_id = shm_id;
    g_queues[slot].owner_pid = owner_pid;
    g_queues[slot].max_messages = max_msgs;
    g_queues[slot].message_size = msg_size;
    g_queues[slot].flags = flags;
    g_queues[slot].head = 0;
    g_queues[slot].tail = 0;
    g_queues[slot].count = 0;
    g_queues[slot].lock = 0;
    g_queues[slot].data = (uint8_t *)data;
    
    g_queue_count++;
    
    return g_next_queue_id++;
}

static int exe_queue_find(uint32_t queue_id) {
    for (int i = 0; i < MAX_QUEUES; i++) {
        if (g_queues[i].shm_id != 0) {
            /* Queue ID is index + 1 */
            if ((uint32_t)(i + 1) == queue_id) {
                return i;
            }
        }
    }
    return -1;
}

static int exe_queue_send(uint32_t queue_id, const void *data, uint32_t size) {
    int slot = exe_queue_find(queue_id);
    if (slot < 0) {
        return EXEC_ERR_NOT_FOUND;
    }
    
    internal_queue_t *q = &g_queues[slot];
    
    if (size > q->message_size) {
        return EXEC_ERR_INVALID;
    }
    
    exe_spinlock_acquire(&q->lock);
    
    if (q->count >= q->max_messages) {
        exe_spinlock_release(&q->lock);
        return EXEC_ERR_QUEUE_FULL;
    }
    
    /* Copy message to queue */
    uint32_t offset = q->tail * q->message_size;
    exe_memcpy(q->data + offset, data, size);
    
    q->tail = (q->tail + 1) % q->max_messages;
    q->count++;
    
    exe_spinlock_release(&q->lock);
    return EXEC_OK;
}

static int exe_queue_receive(uint32_t queue_id, void *data, uint32_t max_size, uint32_t *actual_size) {
    int slot = exe_queue_find(queue_id);
    if (slot < 0) {
        return EXEC_ERR_NOT_FOUND;
    }
    
    internal_queue_t *q = &g_queues[slot];
    
    exe_spinlock_acquire(&q->lock);
    
    if (q->count == 0) {
        exe_spinlock_release(&q->lock);
        return EXEC_ERR_QUEUE_EMPTY;
    }
    
    /* Copy message from queue */
    uint32_t offset = q->head * q->message_size;
    uint32_t copy_size = (max_size < q->message_size) ? max_size : q->message_size;
    exe_memcpy(data, q->data + offset, copy_size);
    *actual_size = copy_size;
    
    q->head = (q->head + 1) % q->max_messages;
    q->count--;
    
    exe_spinlock_release(&q->lock);
    return EXEC_OK;
}

/*=============================================================================
 * REQUEST HANDLERS
 *===========================================================================*/

static void exe_queue_handle_create(const exec_request_t *req, exec_response_t *resp) {
    queue_create_req_t *payload = (queue_create_req_t *)req->payload;
    
    int queue_id = exe_queue_create(payload->name, payload->max_messages,
                                     payload->message_size, payload->flags,
                                     req->sender_pid);
    
    resp->msg_id = req->msg_id;
    resp->status = (queue_id >= 0) ? EXEC_OK : queue_id;
    resp->result = (queue_id >= 0) ? (uint32_t)queue_id : 0;
    resp->payload_size = 0;
}

static void exe_queue_handle_send(const exec_request_t *req, exec_response_t *resp) {
    queue_send_req_t *payload = (queue_send_req_t *)req->payload;
    
    int result = exe_queue_send(payload->queue_id, payload->data, payload->size);
    
    resp->msg_id = req->msg_id;
    resp->status = result;
    resp->result = 0;
    resp->payload_size = 0;
}

static void exe_queue_handle_receive(const exec_request_t *req, exec_response_t *resp) {
    queue_receive_req_t *payload = (queue_receive_req_t *)req->payload;
    queue_receive_resp_t *resp_data = (queue_receive_resp_t *)resp->payload;
    
    uint32_t actual_size = 0;
    int result = exe_queue_receive(payload->queue_id, resp_data->data, 
                                    QUEUE_MSG_MAX_SIZE, &actual_size);
    
    resp->msg_id = req->msg_id;
    resp->status = result;
    resp->result = 0;
    
    if (result == EXEC_OK) {
        resp_data->size = actual_size;
        resp->payload_size = sizeof(uint32_t) + actual_size;
    } else {
        resp->payload_size = 0;
    }
}

static void exe_queue_handle_ping(const exec_request_t *req, exec_response_t *resp) {
    resp->msg_id = req->msg_id;
    resp->status = EXEC_OK;
    resp->result = 1;
    resp->payload_size = 0;
}

/*=============================================================================
 * REQUEST DISPATCHER
 *===========================================================================*/

static void exe_queue_process_request(const exec_request_t *req, exec_response_t *resp) {
    exe_memset(resp, 0, sizeof(exec_response_t));
    
    switch (req->opcode) {
        case EXEC_OP_PING:
            exe_queue_handle_ping(req, resp);
            break;
            
        case EXEC_OP_SHUTDOWN:
            g_ecb->stop_requested = 1;
            resp->msg_id = req->msg_id;
            resp->status = EXEC_OK;
            break;
            
        case QUEUE_OP_CREATE:
            exe_queue_handle_create(req, resp);
            break;
            
        case QUEUE_OP_SEND:
            exe_queue_handle_send(req, resp);
            break;
            
        case QUEUE_OP_RECEIVE:
            exe_queue_handle_receive(req, resp);
            break;
            
        default:
            resp->msg_id = req->msg_id;
            resp->status = EXEC_ERR_INVALID;
            break;
    }
    
    if (resp->status == EXEC_OK) {
        EXEC_STAT_SUCCESS(g_ecb);
    } else {
        EXEC_STAT_FAILURE(g_ecb);
    }
}

/*=============================================================================
 * INITIALIZATION & MAIN LOOP
 *===========================================================================*/

static int exe_queue_init(void) {
    exe_klog(LOG_INFO, "QUEUEEXEC", "Queue Executive initializing...");
    
    /* Initialize queue storage */
    exe_memset(g_queues, 0, sizeof(g_queues));
    g_queue_count = 0;
    g_next_queue_id = 1;
    
    /* Create request queue SHM */
    g_req_queue_shm_id = exe_shm_create(sizeof(exec_request_queue_t));
    if (g_req_queue_shm_id < 0) {
        exe_klog(LOG_ERROR, "QUEUEEXEC", "Failed to create request queue SHM");
        return -1;
    }
    
    g_req_queue = (exec_request_queue_t *)exe_shm_attach(g_req_queue_shm_id);
    if (!g_req_queue) {
        exe_klog(LOG_ERROR, "QUEUEEXEC", "Failed to attach request queue SHM");
        return -1;
    }
    exe_request_queue_init(g_req_queue);
    exe_klog_hex(LOG_INFO, "QUEUEEXEC", "Request queue SHM ID:", g_req_queue_shm_id);
    
    /* Create response queue SHM */
    g_resp_queue_shm_id = exe_shm_create(sizeof(exec_response_queue_t));
    if (g_resp_queue_shm_id < 0) {
        exe_klog(LOG_ERROR, "QUEUEEXEC", "Failed to create response queue SHM");
        return -1;
    }
    
    g_resp_queue = (exec_response_queue_t *)exe_shm_attach(g_resp_queue_shm_id);
    if (!g_resp_queue) {
        exe_klog(LOG_ERROR, "QUEUEEXEC", "Failed to attach response queue SHM");
        return -1;
    }
    exe_response_queue_init(g_resp_queue);
    exe_klog_hex(LOG_INFO, "QUEUEEXEC", "Response queue SHM ID:", g_resp_queue_shm_id);
    
    /* Initialize ECB */
    static exec_control_block_t local_ecb;
    g_ecb = &local_ecb;
    exe_str_copy(g_ecb->name, "queue_executive", EXEC_NAME_MAX);
    g_ecb->exec_id = EXEC_ID_QUEUE;
    g_ecb->state = EXEC_STATE_STARTING;
    g_ecb->priority = PRIORITY_HIGH;
    g_ecb->request_queue_shm_id = g_req_queue_shm_id;
    g_ecb->response_queue_shm_id = g_resp_queue_shm_id;
    
    /* Publish SHM IDs to cells for discovery */
    exe_cell_write("system.exec.queue.req_shm", &g_req_queue_shm_id, sizeof(int));
    exe_cell_write("system.exec.queue.resp_shm", &g_resp_queue_shm_id, sizeof(int));
    
    exe_klog(LOG_INFO, "QUEUEEXEC", "Queue Executive initialized successfully");
    return 0;
}

void exe_queue_main(void) {
    if (exe_queue_init() != 0) {
        exe_klog(LOG_ERROR, "QUEUEEXEC", "Initialization failed! Halting.");
        while (1) __asm__ volatile("hlt");
    }
    
    EXEC_SET_STATE(g_ecb, EXEC_STATE_RUNNING);
    exe_klog(LOG_INFO, "QUEUEEXEC", "Entering main loop...");
    
    exec_request_t req;
    exec_response_t resp;
    
    while (!EXEC_SHOULD_STOP(g_ecb)) {
        if (exe_request_queue_pop(g_req_queue, &req) == EXEC_OK) {
            exe_queue_process_request(&req, &resp);
            exe_response_queue_push(g_resp_queue, &resp);
        } else {
            exe_yield();
        }
    }
    
    EXEC_SET_STATE(g_ecb, EXEC_STATE_STOPPED);
    exe_klog(LOG_WARN, "QUEUEEXEC", "Stopped. Halting.");
    while (1) __asm__ volatile("hlt");
}
