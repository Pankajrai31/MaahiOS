/**
 * MaahiOS Memory Executive Implementation
 * 
 * Description:
 *   Memory Executive provides heap and SHM management services.
 *   Makes syscalls to kernel memory/SHM managers for actual operations.
 *   Uses liblog for logging (Log Executive must be running first).
 *   Uses libcell for cell registration (Cell Executive must be running first).
 * 
 * PID 5 - loaded 4th by sysman (after Log PID 2, Cell PID 3, Process PID 4)
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#include "memory_executive.h"
#include "../common/executive_queue.h"
#include "../../libraries/core/syscall_helpers.h"
#include "../../libraries/liblog/liblog.h"
#include "../../libraries/libcell/libcell.h"

/*=============================================================================
 * CONVENIENCE WRAPPERS (thin layer over syscall_helpers.h)
 *===========================================================================*/

static inline void exe_yield(void) {
    syscall0(SYS_YIELD);
}

static inline int exe_shm_create(uint32_t size) {
    return syscall1(SYS_SHM_CREATE, (int)size);
}

static inline void* exe_shm_attach(int shm_id) {
    return (void*)syscall2(SYS_SHM_ATTACH, shm_id, 0);
}

/*=============================================================================
 * MEMORY KERNEL SYSCALLS (direct - this executive owns memory ops)
 *===========================================================================*/

static inline void* exe_mem_alloc_page(void) {
    return (void*)syscall0(SYS_MEM_ALLOC_PAGE);
}

static inline int exe_mem_free_page(void *addr) {
    return syscall1(SYS_MEM_FREE_PAGE, (int)addr);
}

static inline void* exe_mem_alloc(uint32_t size) {
    return (void*)syscall1(SYS_MEM_ALLOC, (int)size);
}

static inline int exe_shm_detach(void *addr) {
    return syscall1(SYS_SHM_DETACH, (int)addr);
}

static inline int exe_shm_destroy(int shm_id) {
    return syscall1(SYS_SHM_DESTROY, shm_id);
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

static void exe_memory_handle_alloc_page(const exec_request_t *req, exec_response_t *resp) {
    (void)req;
    
    void *page = exe_mem_alloc_page();
    
    resp->msg_id = req->msg_id;
    resp->status = (page != NULL) ? EXEC_OK : EXEC_ERR_NO_MEMORY;
    resp->result = (uint32_t)page;
    resp->payload_size = 0;
    
    if (page) {
        liblog_hex(LOG_DEBUG, "MEMEXEC", "Allocated page at:", (uint32_t)page);
    } else {
        liblog(LOG_ERROR, "MEMEXEC", "Failed to allocate page!");
    }
}

static void exe_memory_handle_free_page(const exec_request_t *req, exec_response_t *resp) {
    mem_free_page_req_t *payload = (mem_free_page_req_t *)req->payload;
    
    liblog_hex(LOG_DEBUG, "MEMEXEC", "Free page at:", payload->address);
    
    int result = exe_mem_free_page((void *)payload->address);
    
    resp->msg_id = req->msg_id;
    resp->status = (result >= 0) ? EXEC_OK : result;
    resp->result = 0;
    resp->payload_size = 0;
}

static void exe_memory_handle_alloc(const exec_request_t *req, exec_response_t *resp) {
    mem_alloc_req_t *payload = (mem_alloc_req_t *)req->payload;
    
    void *addr = exe_mem_alloc(payload->size);
    
    resp->msg_id = req->msg_id;
    resp->status = (addr != NULL) ? EXEC_OK : EXEC_ERR_NO_MEMORY;
    resp->result = (uint32_t)addr;
    resp->payload_size = 0;
    
    if (addr) {
        liblog_hex(LOG_DEBUG, "MEMEXEC", "Allocated block at:", (uint32_t)addr);
    } else {
        liblog(LOG_ERROR, "MEMEXEC", "Failed to allocate block!");
    }
}

static void exe_memory_handle_get_info(const exec_request_t *req, exec_response_t *resp) {
    mem_info_resp_t *resp_data = (mem_info_resp_t *)resp->payload;
    
    /* Use debug syscall to get memory info */
    uint32_t info_buf[3] = {0, 0, 0};
    int result = syscall1(SYS_GET_MEM_INFO, (int)info_buf);
    
    resp->msg_id = req->msg_id;
    resp->status = (result >= 0) ? EXEC_OK : result;
    resp->result = 0;
    
    if (result >= 0) {
        resp_data->info.total_memory = info_buf[0];
        resp_data->info.free_memory  = info_buf[1];
        resp_data->info.used_memory  = info_buf[2];
        resp->payload_size = sizeof(mem_info_resp_t);
    } else {
        resp->payload_size = 0;
    }
}

static void exe_memory_handle_shm_create(const exec_request_t *req, exec_response_t *resp) {
    mem_shm_create_req_t *payload = (mem_shm_create_req_t *)req->payload;
    
    int shm_id = exe_shm_create(payload->size);
    
    resp->msg_id = req->msg_id;
    resp->status = (shm_id >= 0) ? EXEC_OK : EXEC_ERR_NO_MEMORY;
    resp->result = (shm_id >= 0) ? (uint32_t)shm_id : 0;
    resp->payload_size = 0;
    
    if (shm_id >= 0) {
        liblog_hex(LOG_DEBUG, "MEMEXEC", "SHM created, ID:", (uint32_t)shm_id);
    } else {
        liblog(LOG_ERROR, "MEMEXEC", "SHM create failed!");
    }
}

static void exe_memory_handle_shm_attach(const exec_request_t *req, exec_response_t *resp) {
    mem_shm_attach_req_t *payload = (mem_shm_attach_req_t *)req->payload;
    
    void *addr = exe_shm_attach(payload->shm_id);
    
    resp->msg_id = req->msg_id;
    resp->status = (addr != NULL) ? EXEC_OK : EXEC_ERR_INVALID;
    resp->result = (uint32_t)addr;
    resp->payload_size = 0;
}

static void exe_memory_handle_shm_detach(const exec_request_t *req, exec_response_t *resp) {
    mem_shm_detach_req_t *payload = (mem_shm_detach_req_t *)req->payload;
    
    int result = exe_shm_detach((void *)payload->address);
    
    resp->msg_id = req->msg_id;
    resp->status = (result >= 0) ? EXEC_OK : result;
    resp->result = 0;
    resp->payload_size = 0;
}

static void exe_memory_handle_shm_delete(const exec_request_t *req, exec_response_t *resp) {
    mem_shm_delete_req_t *payload = (mem_shm_delete_req_t *)req->payload;
    
    int result = exe_shm_destroy(payload->shm_id);
    
    resp->msg_id = req->msg_id;
    resp->status = (result >= 0) ? EXEC_OK : result;
    resp->result = 0;
    resp->payload_size = 0;
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

static void exe_memory_dispatch(const exec_request_t *req, exec_response_t *resp) {
    exe_memset(resp, 0, sizeof(exec_response_t));
    
    switch (req->func_id) {
        case EXEC_OP_PING:
            exe_memory_handle_ping(req, resp);
            break;
            
        case EXEC_OP_SHUTDOWN:
            g_ecb->stop_requested = 1;
            resp->msg_id = req->msg_id;
            resp->status = EXEC_OK;
            break;
            
        case MEM_OP_ALLOC_PAGE:
            exe_memory_handle_alloc_page(req, resp);
            break;
            
        case MEM_OP_FREE_PAGE:
            exe_memory_handle_free_page(req, resp);
            break;
            
        case MEM_OP_ALLOC:
            exe_memory_handle_alloc(req, resp);
            break;
            
        case MEM_OP_GET_INFO:
            exe_memory_handle_get_info(req, resp);
            break;
            
        case MEM_OP_SHM_CREATE:
            exe_memory_handle_shm_create(req, resp);
            break;
            
        case MEM_OP_SHM_ATTACH:
            exe_memory_handle_shm_attach(req, resp);
            break;
            
        case MEM_OP_SHM_DETACH:
            exe_memory_handle_shm_detach(req, resp);
            break;
            
        case MEM_OP_SHM_DELETE:
            exe_memory_handle_shm_delete(req, resp);
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
    /* liblog auto-inits: if Log Executive is ready, uses SHM queue;
     * if not ready yet, falls back to direct klog syscall. */
    liblog(LOG_INFO, "MEMEXEC", "Memory Executive initializing...");
    
    /* Create SHM queues */
    g_req_queue_shm_id = exe_shm_create(sizeof(exec_request_queue_t));
    if (g_req_queue_shm_id < 0) {
        liblog(LOG_ERROR, "MEMEXEC", "Failed to create request queue SHM");
        return -1;
    }
    
    g_req_queue = (exec_request_queue_t *)exe_shm_attach(g_req_queue_shm_id);
    if (!g_req_queue) {
        liblog(LOG_ERROR, "MEMEXEC", "Failed to attach request queue SHM");
        return -1;
    }
    exe_request_queue_init(g_req_queue);
    liblog_hex(LOG_INFO, "MEMEXEC", "Request queue SHM ID:", g_req_queue_shm_id);
    
    g_resp_queue_shm_id = exe_shm_create(sizeof(exec_response_queue_t));
    if (g_resp_queue_shm_id < 0) {
        liblog(LOG_ERROR, "MEMEXEC", "Failed to create response queue SHM");
        return -1;
    }
    
    g_resp_queue = (exec_response_queue_t *)exe_shm_attach(g_resp_queue_shm_id);
    if (!g_resp_queue) {
        liblog(LOG_ERROR, "MEMEXEC", "Failed to attach response queue SHM");
        return -1;
    }
    exe_response_queue_init(g_resp_queue);
    liblog_hex(LOG_INFO, "MEMEXEC", "Response queue SHM ID:", g_resp_queue_shm_id);
    
    /* Set up control block */
    static exec_control_block_t local_ecb;
    g_ecb = &local_ecb;
    exe_str_copy(g_ecb->name, "memory_executive", EXEC_NAME_MAX);
    g_ecb->exec_id = EXEC_ID_MEMORY;
    g_ecb->state = EXEC_STATE_STARTING;
    g_ecb->priority = PRIORITY_HIGH;
    g_ecb->request_queue_shm_id = g_req_queue_shm_id;
    g_ecb->response_queue_shm_id = g_resp_queue_shm_id;
    
    /* Write queue SHM IDs to cells for discovery (via libcell - auto-inits) */
    libcell_write("system.exec.memory.req_shm", &g_req_queue_shm_id, sizeof(int));
    libcell_write("system.exec.memory.resp_shm", &g_resp_queue_shm_id, sizeof(int));
    
    liblog(LOG_INFO, "MEMEXEC", "Memory Executive initialized successfully");
    return 0;
}

void exe_memory_main(void) {
    if (exe_memory_init() != 0) {
        liblog(LOG_ERROR, "MEMEXEC", "Initialization failed! Halting.");
        while (1) __asm__ volatile("hlt");
    }
    
    EXEC_SET_STATE(g_ecb, EXEC_STATE_RUNNING);
    liblog(LOG_INFO, "MEMEXEC", "Entering main loop...");
    
    exec_request_t req;
    exec_response_t resp;
    
    while (!EXEC_SHOULD_STOP(g_ecb)) {
        if (exe_request_queue_pop(g_req_queue, &req) == EXEC_OK) {
            exe_memory_dispatch(&req, &resp);
            exe_response_queue_push(g_resp_queue, &resp);
        } else {
            exe_yield();
        }
    }
    
    EXEC_SET_STATE(g_ecb, EXEC_STATE_STOPPED);
    liblog(LOG_WARN, "MEMEXEC", "Stopped. Halting.");
    while (1) __asm__ volatile("hlt");
}
