/**
 * MaahiOS Cell Library (libcell) - Implementation
 * 
 * Description:
 *   User-space library for cell registry access.
 *   Auto-initializes on first call (discovers SHM, attaches).
 *   Falls back to direct SYS_CELL_* kernel syscalls if Cell Executive
 *   is not yet running.
 * 
 * Usage:
 *   libcell_write_int("app.volume", 80);  // just call it
 *   int vol; libcell_read_int("app.volume", &vol);
 *   No init() needed — handled automatically.
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#include "libcell.h"
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
 * INTERNAL HELPERS
 *===========================================================================*/

static void _str_copy(char *dest, const char *src, int max) {
    int i = 0;
    while (src && src[i] && i < max - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

/*=============================================================================
 * INTERNAL: AUTO-INIT (lazy, called on first use)
 *===========================================================================*/

/**
 * _libcell_try_init - Try to connect to Cell Executive's SHM queues.
 * Called automatically on first use. Non-fatal if executive not ready.
 * Returns: 0 on success, -1 if executive not available yet.
 */
static int _libcell_try_init(void) {
    if (g_initialized) return 0;
    
    /* Get our PID (only once) and seed msg_id to avoid collisions.
     * Multiple processes share the same executive response queue,
     * so msg_ids must be unique per process. */
    if (g_my_pid == 0) {
        g_my_pid = (uint32_t)syscall0(SYS_GETPID);
        g_msg_id = (g_my_pid << 16) | 1;
    }
    
    /* Read Cell Executive's request queue SHM ID from cell registry */
    int req_shm_id = -1;
    int result = syscall3(SYS_CELL_READ,
                          (uint32_t)"system.exec.cell.req_shm",
                          (uint32_t)&req_shm_id, sizeof(int));
    if (result < 0 || req_shm_id < 0) {
        return -1;  /* Executive not registered yet */
    }
    
    /* Read Cell Executive's response queue SHM ID */
    int resp_shm_id = -1;
    result = syscall3(SYS_CELL_READ,
                      (uint32_t)"system.exec.cell.resp_shm",
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
 * Send a request to Cell Executive and wait for response.
 * Returns EXEC_OK on success, negative error code on timeout/failure.
 */
static int _send_and_wait(exec_request_t *req, exec_response_t *resp) {
    if (!g_req_queue || !g_resp_queue) return EXEC_ERR_NOT_RUNNING;
    
    /* Assign unique ID */
    uint32_t my_id = g_msg_id++;
    req->msg_id     = my_id;
    req->sender_pid = g_my_pid;
    req->exec_id    = EXEC_ID_CELL;
    
    /* Push request */
    int push_result = exe_request_queue_push(g_req_queue, req);
    if (push_result != EXEC_OK) return push_result;
    
    /* Wait for matching response — sleep 1 tick between checks
     * so PIT can switch to Cell Executive to process our request */
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

int libcell_init(void) {
    return _libcell_try_init();
}

void libcell_shutdown(void) {
    g_req_queue  = (void*)0;
    g_resp_queue = (void*)0;
    g_initialized = 0;
}

/*=============================================================================
 * PUBLIC API: CELL OPERATIONS
 * Each function: auto-inits → if connected, use executive → else fallback
 *===========================================================================*/

int libcell_register(const char *name, cell_type_t type, uint32_t flags) {
    /* Auto-init on first call */
    if (!g_initialized) _libcell_try_init();
    
    if (g_initialized) {
        /* Connected to Cell Executive — use SHM queue */
        exec_request_t req;
        exec_response_t resp;
        exe_memset(&req, 0, sizeof(req));
        
        req.func_id = CELL_OP_REGISTER;
        cell_register_req_t *payload = (cell_register_req_t *)req.payload;
        _str_copy(payload->name, name, CELL_NAME_MAX);
        payload->type  = type;
        payload->flags = flags;
        req.payload_size = sizeof(cell_register_req_t);
        
        int result = _send_and_wait(&req, &resp);
        if (result != EXEC_OK) return result;
        return resp.status == EXEC_OK ? (int)resp.result : resp.status;
    }
    
    /* Fallback: write a zero int via direct kernel syscall */
    int32_t zero = 0;
    return syscall3(SYS_CELL_WRITE, (int)name, (int)&zero, sizeof(int32_t));
}

int libcell_write(const char *name, const void *data, uint32_t size) {
    if (!g_initialized) _libcell_try_init();
    
    if (g_initialized) {
        if (size > CELL_DATA_MAX) return EXEC_ERR_INVALID;
        
        exec_request_t req;
        exec_response_t resp;
        exe_memset(&req, 0, sizeof(req));
        
        req.func_id = CELL_OP_WRITE;
        cell_write_req_t *payload = (cell_write_req_t *)req.payload;
        _str_copy(payload->name, name, CELL_NAME_MAX);
        payload->size = size;
        exe_memcpy(payload->data, data, size);
        req.payload_size = sizeof(cell_write_req_t);
        
        int result = _send_and_wait(&req, &resp);
        if (result != EXEC_OK) return result;
        return resp.status;
    }
    
    /* Fallback: direct kernel syscall */
    return syscall3(SYS_CELL_WRITE, (int)name, (int)data, (int)size);
}

int libcell_read(const char *name, void *buffer, uint32_t max_size) {
    if (!g_initialized) _libcell_try_init();
    
    if (g_initialized) {
        exec_request_t req;
        exec_response_t resp;
        exe_memset(&req, 0, sizeof(req));
        
        req.func_id = CELL_OP_READ;
        cell_read_req_t *payload = (cell_read_req_t *)req.payload;
        _str_copy(payload->name, name, CELL_NAME_MAX);
        payload->max_size = max_size;
        req.payload_size = sizeof(cell_read_req_t);
        
        int result = _send_and_wait(&req, &resp);
        if (result != EXEC_OK) return result;
        if (resp.status != EXEC_OK) return resp.status;
        
        cell_read_resp_t *resp_data = (cell_read_resp_t *)resp.payload;
        uint32_t copy_size = resp_data->size;
        if (copy_size > max_size) copy_size = max_size;
        exe_memcpy(buffer, resp_data->data, copy_size);
        return (int)copy_size;
    }
    
    /* Fallback: direct kernel syscall */
    return syscall3(SYS_CELL_READ, (int)name, (int)buffer, (int)max_size);
}

int libcell_write_int(const char *name, int32_t value) {
    if (!g_initialized) _libcell_try_init();
    
    if (g_initialized) {
        exec_request_t req;
        exec_response_t resp;
        exe_memset(&req, 0, sizeof(req));
        
        req.func_id = CELL_OP_WRITE_INT;
        cell_write_int_req_t *payload = (cell_write_int_req_t *)req.payload;
        _str_copy(payload->name, name, CELL_NAME_MAX);
        payload->value = value;
        req.payload_size = sizeof(cell_write_int_req_t);
        
        int result = _send_and_wait(&req, &resp);
        if (result != EXEC_OK) return result;
        return resp.status;
    }
    
    /* Fallback: direct kernel syscall */
    return syscall3(SYS_CELL_WRITE, (int)name, (int)&value, sizeof(int32_t));
}

int libcell_read_int(const char *name, int32_t *value) {
    if (!value) return EXEC_ERR_INVALID;
    if (!g_initialized) _libcell_try_init();
    
    if (g_initialized) {
        exec_request_t req;
        exec_response_t resp;
        exe_memset(&req, 0, sizeof(req));
        
        req.func_id = CELL_OP_READ_INT;
        cell_name_req_t *payload = (cell_name_req_t *)req.payload;
        _str_copy(payload->name, name, CELL_NAME_MAX);
        req.payload_size = sizeof(cell_name_req_t);
        
        int result = _send_and_wait(&req, &resp);
        if (result != EXEC_OK) return result;
        if (resp.status != EXEC_OK) return resp.status;
        
        cell_read_int_resp_t *resp_data = (cell_read_int_resp_t *)resp.payload;
        *value = resp_data->value;
        return EXEC_OK;
    }
    
    /* Fallback: direct kernel syscall */
    int result = syscall3(SYS_CELL_READ, (int)name, (int)value, sizeof(int32_t));
    return (result >= 0) ? 0 : result;
}

int libcell_delete(const char *name) {
    if (!g_initialized) _libcell_try_init();
    
    if (g_initialized) {
        exec_request_t req;
        exec_response_t resp;
        exe_memset(&req, 0, sizeof(req));
        
        req.func_id = CELL_OP_DELETE;
        cell_name_req_t *payload = (cell_name_req_t *)req.payload;
        _str_copy(payload->name, name, CELL_NAME_MAX);
        req.payload_size = sizeof(cell_name_req_t);
        
        int result = _send_and_wait(&req, &resp);
        if (result != EXEC_OK) return result;
        return resp.status;
    }
    
    /* Fallback: direct kernel syscall */
    return syscall1(SYS_CELL_DELETE, (int)name);
}

int libcell_exists(const char *name) {
    if (!g_initialized) _libcell_try_init();
    
    if (g_initialized) {
        exec_request_t req;
        exec_response_t resp;
        exe_memset(&req, 0, sizeof(req));
        
        req.func_id = CELL_OP_EXISTS;
        cell_name_req_t *payload = (cell_name_req_t *)req.payload;
        _str_copy(payload->name, name, CELL_NAME_MAX);
        req.payload_size = sizeof(cell_name_req_t);
        
        int result = _send_and_wait(&req, &resp);
        if (result != EXEC_OK) return result;
        return (resp.status == EXEC_OK) ? 1 : 0;
    }
    
    /* Fallback: direct kernel syscall */
    return syscall1(SYS_CELL_EXISTS, (int)name);
}

int libcell_lookup(const char *name) {
    if (!g_initialized) _libcell_try_init();
    
    if (g_initialized) {
        exec_request_t req;
        exec_response_t resp;
        exe_memset(&req, 0, sizeof(req));
        
        req.func_id = CELL_OP_LOOKUP;
        cell_name_req_t *payload = (cell_name_req_t *)req.payload;
        _str_copy(payload->name, name, CELL_NAME_MAX);
        req.payload_size = sizeof(cell_name_req_t);
        
        int result = _send_and_wait(&req, &resp);
        if (result != EXEC_OK) return result;
        return (resp.status == EXEC_OK) ? (int)resp.result : resp.status;
    }
    
    /* Fallback: exists check via direct kernel syscall */
    return syscall1(SYS_CELL_EXISTS, (int)name);
}

int libcell_get_info(const char *name, cell_info_t *info) {
    if (!info) return EXEC_ERR_INVALID;
    if (!g_initialized) _libcell_try_init();
    
    if (g_initialized) {
        exec_request_t req;
        exec_response_t resp;
        exe_memset(&req, 0, sizeof(req));
        
        req.func_id = CELL_OP_GET_INFO;
        cell_name_req_t *payload = (cell_name_req_t *)req.payload;
        _str_copy(payload->name, name, CELL_NAME_MAX);
        req.payload_size = sizeof(cell_name_req_t);
        
        int result = _send_and_wait(&req, &resp);
        if (result != EXEC_OK) return result;
        if (resp.status != EXEC_OK) return resp.status;
        
        cell_info_resp_t *resp_data = (cell_info_resp_t *)resp.payload;
        exe_memcpy(info, &resp_data->info, sizeof(cell_info_t));
        return EXEC_OK;
    }
    
    /* Fallback: not available without executive — return error */
    return EXEC_ERR_NOT_RUNNING;
}
