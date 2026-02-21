/**
 * MaahiOS Memory Executive Implementation
 * 
 * Description:
 *   Memory Executive provides heap and SHM management services.
 *   Makes syscalls to kernel memory managers.
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#include "memory_executive.h"
#include "../common/executive_queue.h"

/*=============================================================================
 * SYSCALL INTERFACE
 *===========================================================================*/

#define SYS_YIELD           1
#define SYS_MEM_ALLOC       32
#define SYS_MEM_FREE        33
#define SYS_MEM_REALLOC     34
#define SYS_MEM_INFO        35
#define SYS_SHM_CREATE      48
#define SYS_SHM_ATTACH      49
#define SYS_SHM_DETACH      50
#define SYS_SHM_DELETE      51
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

static inline int syscall2(int num, int a1, int a2) {
    return syscall5(num, a1, a2, 0, 0, 0);
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

static inline int exe_shm_detach(void *addr) {
    return syscall1(SYS_SHM_DETACH, (int)addr);
}

static inline int exe_shm_delete(int shm_id) {
    return syscall1(SYS_SHM_DELETE, shm_id);
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

static inline void* exe_mem_alloc(uint32_t size) {
    return (void*)syscall1(SYS_MEM_ALLOC, (int)size);
}

static inline int exe_mem_free(void *addr) {
    return syscall1(SYS_MEM_FREE, (int)addr);
}

/*=============================================================================
 * EXECUTIVE STATE
 *===========================================================================*/

static exec_control_block_t *g_ecb = NULL;
static exec_request_queue_t *g_req_queue = NULL;
static exec_response_queue_t *g_resp_queue = NULL;
static int g_req_queue_shm_id = -1;
static int g_resp_queue_shm_id = -1;

/*=============================================================================
 * REQUEST HANDLERS
 *===========================================================================*/

static void exe_memory_handle_alloc(const exec_request_t *req, exec_response_t *resp) {
    mem_alloc_req_t *payload = (mem_alloc_req_t *)req->payload;
    mem_alloc_resp_t *resp_data = (mem_alloc_resp_t *)resp->payload;
    
    void *addr = exe_mem_alloc(payload->size);
    
    resp->msg_id = req->msg_id;
    resp->status = (addr != NULL) ? EXEC_OK : EXEC_ERR_NO_MEMORY;
    resp->result = 0;
    
    if (addr) {
        resp_data->address = (uint32_t)addr;
        resp->payload_size = sizeof(mem_alloc_resp_t);
    } else {
        resp->payload_size = 0;
    }
}

static void exe_memory_handle_free(const exec_request_t *req, exec_response_t *resp) {
    mem_free_req_t *payload = (mem_free_req_t *)req->payload;
    
    int result = exe_mem_free((void *)payload->address);
    
    resp->msg_id = req->msg_id;
    resp->status = (result >= 0) ? EXEC_OK : result;
    resp->result = 0;
    resp->payload_size = 0;
}

static void exe_memory_handle_shm_create(const exec_request_t *req, exec_response_t *resp) {
    mem_shm_create_req_t *payload = (mem_shm_create_req_t *)req->payload;
    mem_shm_create_resp_t *resp_data = (mem_shm_create_resp_t *)resp->payload;
    
    int shm_id = exe_shm_create(payload->size);
    
    resp->msg_id = req->msg_id;
    resp->status = (shm_id >= 0) ? EXEC_OK : EXEC_ERR_NO_MEMORY;
    resp->result = 0;
    
    if (shm_id >= 0) {
        resp_data->shm_id = shm_id;
        resp->payload_size = sizeof(mem_shm_create_resp_t);
    } else {
        resp->payload_size = 0;
    }
}

static void exe_memory_handle_shm_attach(const exec_request_t *req, exec_response_t *resp) {
    mem_shm_attach_req_t *payload = (mem_shm_attach_req_t *)req->payload;
    mem_shm_attach_resp_t *resp_data = (mem_shm_attach_resp_t *)resp->payload;
    
    void *addr = exe_shm_attach(payload->shm_id);
    
    resp->msg_id = req->msg_id;
    resp->status = (addr != NULL) ? EXEC_OK : EXEC_ERR_INVALID;
    resp->result = 0;
    
    if (addr) {
        resp_data->address = (uint32_t)addr;
        resp->payload_size = sizeof(mem_shm_attach_resp_t);
    } else {
        resp->payload_size = 0;
    }
}

static void exe_memory_handle_ping(const exec_request_t *req, exec_response_t *resp) {
    resp->msg_id = req->msg_id;
    resp->status = EXEC_OK;
    resp->result = 1;
    resp->payload_size = 0;
}

/*=============================================================================
 * REQUEST DISPATCHER
 *===========================================================================*/

static void exe_memory_process_request(const exec_request_t *req, exec_response_t *resp) {
    exe_memset(resp, 0, sizeof(exec_response_t));
    
    switch (req->opcode) {
        case EXEC_OP_PING:
            exe_memory_handle_ping(req, resp);
            break;
            
        case EXEC_OP_SHUTDOWN:
            g_ecb->stop_requested = 1;
            resp->msg_id = req->msg_id;
            resp->status = EXEC_OK;
            break;
            
        case MEM_OP_ALLOC:
            exe_memory_handle_alloc(req, resp);
            break;
            
        case MEM_OP_FREE:
            exe_memory_handle_free(req, resp);
            break;
            
        case MEM_OP_SHM_CREATE:
            exe_memory_handle_shm_create(req, resp);
            break;
            
        case MEM_OP_SHM_ATTACH:
            exe_memory_handle_shm_attach(req, resp);
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

static int exe_memory_init(void) {
    exe_klog(LOG_INFO, "MEMEXEC", "Memory Executive initializing...");
    
    g_req_queue_shm_id = exe_shm_create(sizeof(exec_request_queue_t));
    if (g_req_queue_shm_id < 0) {
        exe_klog(LOG_ERROR, "MEMEXEC", "Failed to create request queue SHM");
        return -1;
    }
    
    g_req_queue = (exec_request_queue_t *)exe_shm_attach(g_req_queue_shm_id);
    if (!g_req_queue) {
        exe_klog(LOG_ERROR, "MEMEXEC", "Failed to attach request queue SHM");
        return -1;
    }
    exe_request_queue_init(g_req_queue);
    exe_klog_hex(LOG_INFO, "MEMEXEC", "Request queue SHM ID:", g_req_queue_shm_id);
    
    g_resp_queue_shm_id = exe_shm_create(sizeof(exec_response_queue_t));
    if (g_resp_queue_shm_id < 0) {
        exe_klog(LOG_ERROR, "MEMEXEC", "Failed to create response queue SHM");
        return -1;
    }
    
    g_resp_queue = (exec_response_queue_t *)exe_shm_attach(g_resp_queue_shm_id);
    if (!g_resp_queue) {
        exe_klog(LOG_ERROR, "MEMEXEC", "Failed to attach response queue SHM");
        return -1;
    }
    exe_response_queue_init(g_resp_queue);
    exe_klog_hex(LOG_INFO, "MEMEXEC", "Response queue SHM ID:", g_resp_queue_shm_id);
    
    static exec_control_block_t local_ecb;
    g_ecb = &local_ecb;
    exe_str_copy(g_ecb->name, "memory_executive", EXEC_NAME_MAX);
    g_ecb->exec_id = EXEC_ID_MEMORY;
    g_ecb->state = EXEC_STATE_STARTING;
    g_ecb->priority = PRIORITY_HIGH;
    g_ecb->request_queue_shm_id = g_req_queue_shm_id;
    g_ecb->response_queue_shm_id = g_resp_queue_shm_id;
    
    /* Publish SHM IDs to cells for discovery */
    exe_cell_write("system.exec.memory.req_shm", &g_req_queue_shm_id, sizeof(int));
    exe_cell_write("system.exec.memory.resp_shm", &g_resp_queue_shm_id, sizeof(int));
    
    exe_klog(LOG_INFO, "MEMEXEC", "Memory Executive initialized successfully");
    return 0;
}

void exe_memory_main(void) {
    if (exe_memory_init() != 0) {
        exe_klog(LOG_ERROR, "MEMEXEC", "Initialization failed! Halting.");
        while (1) __asm__ volatile("hlt");
    }
    
    EXEC_SET_STATE(g_ecb, EXEC_STATE_RUNNING);
    exe_klog(LOG_INFO, "MEMEXEC", "Entering main loop...");
    
    exec_request_t req;
    exec_response_t resp;
    
    while (!EXEC_SHOULD_STOP(g_ecb)) {
        if (exe_request_queue_pop(g_req_queue, &req) == EXEC_OK) {
            exe_memory_process_request(&req, &resp);
            exe_response_queue_push(g_resp_queue, &resp);
        } else {
            exe_yield();
        }
    }
    
    EXEC_SET_STATE(g_ecb, EXEC_STATE_STOPPED);
    exe_klog(LOG_WARN, "MEMEXEC", "Stopped. Halting.");
    while (1) __asm__ volatile("hlt");
}
