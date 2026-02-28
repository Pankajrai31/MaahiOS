/**
 * MaahiOS Cell Executive Implementation
 * 
 * Description:
 *   Cell Executive manages the cell registry - a hierarchical key-value store.
 *   Makes syscalls to kernel cell_manager for actual storage.
 *   Uses liblog for logging (Log Executive must be running first).
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#include "cell_executive.h"
#include "../common/executive_queue.h"
#include "../../libraries/core/syscall_helpers.h"
#include "../../libraries/liblog/liblog.h"

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
 * CELL KERNEL SYSCALLS
 *===========================================================================*/

static inline int exe_cell_write(const char *name, const void *data, uint32_t size) {
    return syscall3(SYS_CELL_WRITE, (int)name, (int)data, (int)size);
}

static inline int exe_cell_read(const char *name, void *buffer, uint32_t max_size) {
    return syscall3(SYS_CELL_READ, (int)name, (int)buffer, (int)max_size);
}

static inline int exe_cell_delete(const char *name) {
    return syscall1(SYS_CELL_DELETE, (int)name);
}

static inline int exe_cell_exists(const char *name) {
    return syscall1(SYS_CELL_EXISTS, (int)name);
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

static void exe_cell_handle_register(const exec_request_t *req, exec_response_t *resp) {
    cell_register_req_t *payload = (cell_register_req_t *)req->payload;
    
    /* cell_write auto-creates if key doesn't exist, just write a zero int for register */
    int32_t zero = 0;
    int result = exe_cell_write(payload->name, &zero, sizeof(int32_t));
    
    resp->msg_id = req->msg_id;
    resp->status = (result >= 0) ? EXEC_OK : result;
    resp->result = (result >= 0) ? (uint32_t)result : 0;
    resp->payload_size = 0;
}

static void exe_cell_handle_write(const exec_request_t *req, exec_response_t *resp) {
    cell_write_req_t *payload = (cell_write_req_t *)req->payload;
    
    int result = exe_cell_write(payload->name, payload->data, payload->size);
    
    resp->msg_id = req->msg_id;
    resp->status = (result >= 0) ? EXEC_OK : result;
    resp->result = 0;
    resp->payload_size = 0;
}

static void exe_cell_handle_read(const exec_request_t *req, exec_response_t *resp) {
    cell_read_req_t *payload = (cell_read_req_t *)req->payload;
    cell_read_resp_t *resp_data = (cell_read_resp_t *)resp->payload;
    
    uint32_t max_size = payload->max_size;
    if (max_size > CELL_DATA_MAX) max_size = CELL_DATA_MAX;
    
    int result = exe_cell_read(payload->name, resp_data->data, max_size);
    
    resp->msg_id = req->msg_id;
    resp->status = (result >= 0) ? EXEC_OK : result;
    resp->result = 0;
    
    if (result >= 0) {
        resp_data->size = (uint32_t)result;
        resp->payload_size = sizeof(uint32_t) + result;
    } else {
        resp->payload_size = 0;
    }
}

static void exe_cell_handle_write_int(const exec_request_t *req, exec_response_t *resp) {
    cell_write_int_req_t *payload = (cell_write_int_req_t *)req->payload;
    
    int result = exe_cell_write(payload->name, &payload->value, sizeof(int32_t));
    
    resp->msg_id = req->msg_id;
    resp->status = (result >= 0) ? EXEC_OK : result;
    resp->result = 0;
    resp->payload_size = 0;
}

static void exe_cell_handle_read_int(const exec_request_t *req, exec_response_t *resp) {
    cell_name_req_t *payload = (cell_name_req_t *)req->payload;
    cell_read_int_resp_t *resp_data = (cell_read_int_resp_t *)resp->payload;
    
    int32_t value = 0;
    int result = exe_cell_read(payload->name, &value, sizeof(int32_t));
    
    resp->msg_id = req->msg_id;
    resp->status = (result >= 0) ? EXEC_OK : result;
    resp->result = 0;
    
    if (result >= 0) {
        resp_data->value = value;
        resp->payload_size = sizeof(cell_read_int_resp_t);
    } else {
        resp->payload_size = 0;
    }
}

static void exe_cell_handle_delete(const exec_request_t *req, exec_response_t *resp) {
    cell_name_req_t *payload = (cell_name_req_t *)req->payload;
    
    int result = exe_cell_delete(payload->name);
    
    resp->msg_id = req->msg_id;
    resp->status = (result >= 0) ? EXEC_OK : result;
    resp->result = 0;
    resp->payload_size = 0;
}

static void exe_cell_handle_lookup(const exec_request_t *req, exec_response_t *resp) {
    cell_name_req_t *payload = (cell_name_req_t *)req->payload;
    
    int result = exe_cell_exists(payload->name);
    
    resp->msg_id = req->msg_id;
    resp->status = (result > 0) ? EXEC_OK : EXEC_ERR_NOT_FOUND;
    resp->result = (result > 0) ? 1 : 0;
    resp->payload_size = 0;
}

static void exe_cell_handle_exists(const exec_request_t *req, exec_response_t *resp) {
    cell_name_req_t *payload = (cell_name_req_t *)req->payload;
    
    int result = exe_cell_exists(payload->name);
    
    resp->msg_id = req->msg_id;
    resp->status = (result > 0) ? EXEC_OK : EXEC_ERR_NOT_FOUND;
    resp->result = (result > 0) ? 1 : 0;
    resp->payload_size = 0;
}

static void exe_cell_handle_get_info(const exec_request_t *req, exec_response_t *resp) {
    cell_name_req_t *payload = (cell_name_req_t *)req->payload;
    cell_info_resp_t *resp_data = (cell_info_resp_t *)resp->payload;
    
    /* Check if cell exists first */
    int exists = exe_cell_exists(payload->name);
    if (exists <= 0) {
        resp->msg_id = req->msg_id;
        resp->status = EXEC_ERR_NOT_FOUND;
        resp->payload_size = 0;
        return;
    }
    
    /* Read cell data to get size */
    uint8_t tmp[4];
    int read_result = exe_cell_read(payload->name, tmp, sizeof(tmp));
    
    /* Fill in info structure */
    exe_str_copy(resp_data->info.name, payload->name, CELL_NAME_MAX);
    resp_data->info.type = CELL_TYPE_DATA;
    resp_data->info.flags = 0;
    resp_data->info.size = (read_result >= 0) ? (uint32_t)read_result : 0;
    resp_data->info.owner_pid = 0;
    resp_data->info.created_time = 0;
    resp_data->info.modified_time = 0;
    
    resp->msg_id = req->msg_id;
    resp->status = EXEC_OK;
    resp->result = 0;
    resp->payload_size = sizeof(cell_info_resp_t);
}

static void exe_cell_handle_ping(const exec_request_t *req, exec_response_t *resp) {
    resp->msg_id = req->msg_id;
    resp->status = EXEC_OK;
    resp->result = 1;
    resp->payload_size = 0;
}

/*=============================================================================
 * REQUEST DISPATCHER
 *===========================================================================*/

static void exe_cell_process_request(const exec_request_t *req, exec_response_t *resp) {
    exe_memset(resp, 0, sizeof(exec_response_t));
    
    switch (req->func_id) {
        case CELL_OP_REGISTER:
            exe_cell_handle_register(req, resp);
            break;
            
        case CELL_OP_WRITE:
            exe_cell_handle_write(req, resp);
            break;
            
        case CELL_OP_READ:
            exe_cell_handle_read(req, resp);
            break;
            
        case CELL_OP_WRITE_INT:
            exe_cell_handle_write_int(req, resp);
            break;
            
        case CELL_OP_READ_INT:
            exe_cell_handle_read_int(req, resp);
            break;
            
        case CELL_OP_DELETE:
            exe_cell_handle_delete(req, resp);
            break;
            
        case CELL_OP_LOOKUP:
            exe_cell_handle_lookup(req, resp);
            break;
            
        case CELL_OP_EXISTS:
            exe_cell_handle_exists(req, resp);
            break;
            
        case CELL_OP_GET_INFO:
            exe_cell_handle_get_info(req, resp);
            break;
            
        case EXEC_OP_PING:
            exe_cell_handle_ping(req, resp);
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

static int exe_cell_init(void) {
    /* liblog auto-inits: if Log Executive is ready, uses SHM queue;
     * if not ready yet, falls back to direct klog syscall. */
    liblog(LOG_INFO, "CELLEXEC", "Cell Executive initializing...");
    
    /* Create SHM queues */
    g_req_queue_shm_id = exe_shm_create(sizeof(exec_request_queue_t));
    if (g_req_queue_shm_id < 0) {
        liblog(LOG_ERROR, "CELLEXEC", "Failed to create request queue SHM");
        return -1;
    }
    
    g_req_queue = (exec_request_queue_t *)exe_shm_attach(g_req_queue_shm_id);
    if (!g_req_queue) {
        liblog(LOG_ERROR, "CELLEXEC", "Failed to attach request queue SHM");
        return -1;
    }
    exe_request_queue_init(g_req_queue);
    liblog_hex(LOG_INFO, "CELLEXEC", "Request queue SHM ID:", g_req_queue_shm_id);
    
    g_resp_queue_shm_id = exe_shm_create(sizeof(exec_response_queue_t));
    if (g_resp_queue_shm_id < 0) {
        liblog(LOG_ERROR, "CELLEXEC", "Failed to create response queue SHM");
        return -1;
    }
    
    g_resp_queue = (exec_response_queue_t *)exe_shm_attach(g_resp_queue_shm_id);
    if (!g_resp_queue) {
        liblog(LOG_ERROR, "CELLEXEC", "Failed to attach response queue SHM");
        return -1;
    }
    exe_response_queue_init(g_resp_queue);
    liblog_hex(LOG_INFO, "CELLEXEC", "Response queue SHM ID:", g_resp_queue_shm_id);
    
    /* Set up control block */
    static exec_control_block_t local_ecb;
    g_ecb = &local_ecb;
    exe_str_copy(g_ecb->name, "cell_executive", EXEC_NAME_MAX);
    g_ecb->exec_id = EXEC_ID_CELL;
    g_ecb->state = EXEC_STATE_STARTING;
    g_ecb->priority = PRIORITY_HIGH;
    g_ecb->request_queue_shm_id = g_req_queue_shm_id;
    g_ecb->response_queue_shm_id = g_resp_queue_shm_id;
    
    /* Write queue SHM IDs to cells for discovery */
    exe_cell_write("system.exec.cell.req_shm", &g_req_queue_shm_id, sizeof(int));
    exe_cell_write("system.exec.cell.resp_shm", &g_resp_queue_shm_id, sizeof(int));
    
    liblog(LOG_INFO, "CELLEXEC", "Cell Executive initialized successfully");
    return 0;
}

void exe_cell_main(void) {
    if (exe_cell_init() != 0) {
        liblog(LOG_ERROR, "CELLEXEC", "Initialization failed! Halting.");
        while (1) __asm__ volatile("hlt");
    }
    
    EXEC_SET_STATE(g_ecb, EXEC_STATE_RUNNING);
    liblog(LOG_INFO, "CELLEXEC", "Entering main loop...");
    
    exec_request_t req;
    exec_response_t resp;
    
    while (!EXEC_SHOULD_STOP(g_ecb)) {
        if (exe_request_queue_pop(g_req_queue, &req) == EXEC_OK) {
            exe_cell_process_request(&req, &resp);
            exe_response_queue_push(g_resp_queue, &resp);
        } else {
            exe_yield();
        }
    }
    
    EXEC_SET_STATE(g_ecb, EXEC_STATE_STOPPED);
    liblog(LOG_WARN, "CELLEXEC", "Stopped. Halting.");
    while (1) __asm__ volatile("hlt");
}
