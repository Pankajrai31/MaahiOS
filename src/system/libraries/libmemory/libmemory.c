/**
 * MaahiOS Memory Library (libmemory) - Implementation
 * 
 * Description:
 *   User-space library for memory management.
 *   Auto-initializes on first call (discovers SHM, attaches).
 *   Falls back to direct SYS_MEM_* / SYS_SHM_* kernel syscalls if
 *   Memory Executive is not yet running.
 * 
 * Usage:
 *   void *page = libmem_alloc_page();  // just call it
 *   No init() needed — handled automatically.
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#include "libmemory.h"
#include "../core/syscall_helpers.h"
#include "../../executives/common/executive_queue.h"

/*=============================================================================
 * LIBRARY STATE
 *===========================================================================*/

static exec_request_queue_t  *g_req_queue  = (void*)0;
static exec_response_queue_t *g_resp_queue = (void*)0;
static uint32_t g_msg_id      = 1;
static int      g_initialized = 0;
static uint32_t g_my_pid      = 0;

/*=============================================================================
 * INTERNAL: AUTO-INIT (lazy, called on first use)
 *===========================================================================*/

/**
 * _libmemory_try_init - Try to connect to Memory Executive's SHM queues.
 * Called automatically on first use. Non-fatal if executive not ready.
 * Returns: 0 on success, -1 if executive not available yet.
 */
static int _libmemory_try_init(void) {
    if (g_initialized) return 0;
    
    /* Get our PID (only once) and seed msg_id to avoid collisions.
     * Multiple processes share the same executive response queue,
     * so msg_ids must be unique per process. */
    if (g_my_pid == 0) {
        g_my_pid = (uint32_t)syscall0(SYS_GETPID);
        g_msg_id = (g_my_pid << 16) | 1;
    }
    
    /* Read Memory Executive's request queue SHM ID from cell registry */
    int req_shm_id = -1;
    int result = syscall3(SYS_CELL_READ,
                          (uint32_t)"system.exec.memory.req_shm",
                          (uint32_t)&req_shm_id, sizeof(int));
    if (result < 0 || req_shm_id < 0) {
        return -1;  /* Executive not registered yet */
    }
    
    /* Read Memory Executive's response queue SHM ID */
    int resp_shm_id = -1;
    result = syscall3(SYS_CELL_READ,
                      (uint32_t)"system.exec.memory.resp_shm",
                      (uint32_t)&resp_shm_id, sizeof(int));
    if (result < 0 || resp_shm_id < 0) {
        return -1;
    }
    
    /* Attach to request queue SHM */
    g_req_queue = (exec_request_queue_t *)syscall2(SYS_SHM_ATTACH, req_shm_id, 0);
    if (!g_req_queue || (uint32_t)g_req_queue == 0xFFFFFFFF) {
        g_req_queue = (void*)0;
        return -1;
    }
    
    /* Attach to response queue SHM */
    g_resp_queue = (exec_response_queue_t *)syscall2(SYS_SHM_ATTACH, resp_shm_id, 0);
    if (!g_resp_queue || (uint32_t)g_resp_queue == 0xFFFFFFFF) {
        g_resp_queue = (void*)0;
        return -1;
    }
    
    g_initialized = 1;
    return 0;
}

/**
 * Send a request to Memory Executive and wait for response.
 * Returns EXEC_OK on success, negative error code on timeout/failure.
 */
static int _send_and_wait(exec_request_t *req, exec_response_t *resp) {
    if (!g_req_queue || !g_resp_queue) return EXEC_ERR_NOT_RUNNING;
    
    /* Assign unique ID */
    uint32_t my_id = g_msg_id++;
    req->msg_id     = my_id;
    req->sender_pid = g_my_pid;
    req->exec_id    = EXEC_ID_MEMORY;
    
    /* Push request */
    int push_result = exe_request_queue_push(g_req_queue, req);
    if (push_result != EXEC_OK) return push_result;
    
    /* Wait for matching response — sleep 1 tick between checks
     * so PIT can switch to Memory Executive to process our request */
    for (int i = 0; i < 100; i++) {
        int pop_result = exe_response_queue_pop_by_id(g_resp_queue, my_id, resp);
        if (pop_result == EXEC_OK) {
            return EXEC_OK;
        }
        /* Sleep 1 PIT tick (~20ms at 50Hz) to yield CPU */
        syscall1(SYS_SLEEP, 1);
    }
    
    return EXEC_ERR_TIMEOUT;
}

/*=============================================================================
 * PUBLIC API: INIT / SHUTDOWN (optional — auto-init handles it)
 *===========================================================================*/

int libmemory_init(void) {
    return _libmemory_try_init();
}

void libmemory_shutdown(void) {
    g_req_queue  = (void*)0;
    g_resp_queue = (void*)0;
    g_initialized = 0;
}

/*=============================================================================
 * PUBLIC API: PAGE OPERATIONS
 * Each function: auto-inits → if connected, use executive → else fallback
 *===========================================================================*/

void *libmem_alloc_page(void) {
    /* Auto-init on first call */
    if (!g_initialized) _libmemory_try_init();
    
    if (g_initialized) {
        /* Connected to Memory Executive — use SHM queue */
        exec_request_t req;
        exec_response_t resp;
        exe_memset(&req, 0, sizeof(req));
        
        req.func_id = MEM_OP_ALLOC_PAGE;
        req.payload_size = 0;
        
        int result = _send_and_wait(&req, &resp);
        if (result != EXEC_OK) return (void*)0;
        return (resp.status == EXEC_OK) ? (void*)resp.result : (void*)0;
    }
    
    /* Fallback: direct kernel syscall */
    return (void*)syscall0(SYS_MEM_ALLOC_PAGE);
}

int libmem_free_page(void *ptr) {
    if (!ptr) return EXEC_ERR_INVALID;
    if (!g_initialized) _libmemory_try_init();
    
    if (g_initialized) {
        exec_request_t req;
        exec_response_t resp;
        exe_memset(&req, 0, sizeof(req));
        
        req.func_id = MEM_OP_FREE_PAGE;
        mem_free_page_req_t *payload = (mem_free_page_req_t *)req.payload;
        payload->address = (uint32_t)ptr;
        req.payload_size = sizeof(mem_free_page_req_t);
        
        int result = _send_and_wait(&req, &resp);
        if (result != EXEC_OK) return result;
        return resp.status;
    }
    
    /* Fallback: direct kernel syscall */
    return syscall1(SYS_MEM_FREE_PAGE, (int)ptr);
}

/*=============================================================================
 * PUBLIC API: BLOCK ALLOCATION
 *===========================================================================*/

void *libmem_alloc(uint32_t size) {
    if (size == 0) return (void*)0;
    if (!g_initialized) _libmemory_try_init();
    
    if (g_initialized) {
        exec_request_t req;
        exec_response_t resp;
        exe_memset(&req, 0, sizeof(req));
        
        req.func_id = MEM_OP_ALLOC;
        mem_alloc_req_t *payload = (mem_alloc_req_t *)req.payload;
        payload->size = size;
        req.payload_size = sizeof(mem_alloc_req_t);
        
        int result = _send_and_wait(&req, &resp);
        if (result != EXEC_OK) return (void*)0;
        return (resp.status == EXEC_OK) ? (void*)resp.result : (void*)0;
    }
    
    /* Fallback: direct kernel syscall */
    return (void*)syscall1(SYS_MEM_ALLOC, (int)size);
}

/*=============================================================================
 * PUBLIC API: SHARED MEMORY OPERATIONS
 *===========================================================================*/

int libmem_shm_create(uint32_t size) {
    if (!g_initialized) _libmemory_try_init();
    
    if (g_initialized) {
        exec_request_t req;
        exec_response_t resp;
        exe_memset(&req, 0, sizeof(req));
        
        req.func_id = MEM_OP_SHM_CREATE;
        mem_shm_create_req_t *payload = (mem_shm_create_req_t *)req.payload;
        payload->size = size;
        req.payload_size = sizeof(mem_shm_create_req_t);
        
        int result = _send_and_wait(&req, &resp);
        if (result != EXEC_OK) return result;
        return (resp.status == EXEC_OK) ? (int)resp.result : resp.status;
    }
    
    /* Fallback: direct kernel syscall */
    return syscall1(SYS_SHM_CREATE, (int)size);
}

void *libmem_shm_attach(int shm_id) {
    if (!g_initialized) _libmemory_try_init();
    
    if (g_initialized) {
        exec_request_t req;
        exec_response_t resp;
        exe_memset(&req, 0, sizeof(req));
        
        req.func_id = MEM_OP_SHM_ATTACH;
        mem_shm_attach_req_t *payload = (mem_shm_attach_req_t *)req.payload;
        payload->shm_id = shm_id;
        req.payload_size = sizeof(mem_shm_attach_req_t);
        
        int result = _send_and_wait(&req, &resp);
        if (result != EXEC_OK) return (void*)0;
        return (resp.status == EXEC_OK) ? (void*)resp.result : (void*)0;
    }
    
    /* Fallback: direct kernel syscall */
    return (void*)syscall2(SYS_SHM_ATTACH, shm_id, 0);
}

int libmem_shm_detach(void *ptr) {
    if (!ptr) return EXEC_ERR_INVALID;
    if (!g_initialized) _libmemory_try_init();
    
    if (g_initialized) {
        exec_request_t req;
        exec_response_t resp;
        exe_memset(&req, 0, sizeof(req));
        
        req.func_id = MEM_OP_SHM_DETACH;
        mem_shm_detach_req_t *payload = (mem_shm_detach_req_t *)req.payload;
        payload->address = (uint32_t)ptr;
        req.payload_size = sizeof(mem_shm_detach_req_t);
        
        int result = _send_and_wait(&req, &resp);
        if (result != EXEC_OK) return result;
        return resp.status;
    }
    
    /* Fallback: direct kernel syscall */
    return syscall1(SYS_SHM_DETACH, (int)ptr);
}

int libmem_shm_delete(int shm_id) {
    if (!g_initialized) _libmemory_try_init();
    
    if (g_initialized) {
        exec_request_t req;
        exec_response_t resp;
        exe_memset(&req, 0, sizeof(req));
        
        req.func_id = MEM_OP_SHM_DELETE;
        mem_shm_delete_req_t *payload = (mem_shm_delete_req_t *)req.payload;
        payload->shm_id = shm_id;
        req.payload_size = sizeof(mem_shm_delete_req_t);
        
        int result = _send_and_wait(&req, &resp);
        if (result != EXEC_OK) return result;
        return resp.status;
    }
    
    /* Fallback: direct kernel syscall */
    return syscall1(SYS_SHM_DESTROY, shm_id);
}

/*=============================================================================
 * PUBLIC API: MEMORY INFO
 *===========================================================================*/

int libmem_get_info(memory_info_t *info) {
    if (!info) return EXEC_ERR_INVALID;
    if (!g_initialized) _libmemory_try_init();
    
    if (g_initialized) {
        exec_request_t req;
        exec_response_t resp;
        exe_memset(&req, 0, sizeof(req));
        
        req.func_id = MEM_OP_GET_INFO;
        req.payload_size = 0;
        
        int result = _send_and_wait(&req, &resp);
        if (result != EXEC_OK) return result;
        if (resp.status != EXEC_OK) return resp.status;
        
        mem_info_resp_t *resp_data = (mem_info_resp_t *)resp.payload;
        info->total_memory = resp_data->info.total_memory;
        info->free_memory  = resp_data->info.free_memory;
        info->used_memory  = resp_data->info.used_memory;
        return EXEC_OK;
    }
    
    /* Fallback: direct kernel syscall */
    uint32_t info_buf[3] = {0, 0, 0};
    int result = syscall1(SYS_GET_MEM_INFO, (int)info_buf);
    if (result >= 0) {
        info->total_memory = info_buf[0];
        info->free_memory  = info_buf[1];
        info->used_memory  = info_buf[2];
    }
    return result;
}
