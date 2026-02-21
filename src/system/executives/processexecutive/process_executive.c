/**
 * MaahiOS Process Executive Implementation
 * 
 * Description:
 *   Process Executive provides process management services.
 *   Makes syscalls to kernel process_manager.
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#include "process_executive.h"
#include "../common/executive_queue.h"

/*=============================================================================
 * SYSCALL INTERFACE
 *===========================================================================*/

#define SYS_YIELD           1
#define SYS_PROC_CREATE     16
#define SYS_PROC_TERMINATE  17
#define SYS_PROC_GET_PID    18
#define SYS_PROC_GET_INFO   19
#define SYS_PROC_LIST       20
#define SYS_SHM_CREATE      48
#define SYS_SHM_ATTACH      49
#define SYS_CELL_WRITE      64
#define SYS_MOD_GET_ADDR    98
#define SYS_MOD_COPY        100
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

static inline int exe_proc_create(uint32_t entry_point) {
    return syscall1(SYS_PROC_CREATE, (int)entry_point);
}

static inline int exe_proc_terminate(uint32_t pid) {
    return syscall1(SYS_PROC_TERMINATE, (int)pid);
}

static inline int exe_proc_get_pid(void) {
    return syscall0(SYS_PROC_GET_PID);
}

static inline uint32_t exe_mod_get_addr(int index) {
    return (uint32_t)syscall1(SYS_MOD_GET_ADDR, index);
}

static inline int exe_mod_copy(int index, uint32_t target) {
    return syscall2(SYS_MOD_COPY, index, (int)target);
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
 * PROCESS OPERATIONS
 *===========================================================================*/

static int exe_process_create_from_module(uint32_t module_index, uint32_t target_addr) {
    /* Copy module to target address */
    int result = exe_mod_copy((int)module_index, target_addr);
    if (result < 0) {
        return result;
    }
    
    /* Create process at target address */
    return exe_proc_create(target_addr);
}

/*=============================================================================
 * REQUEST HANDLERS
 *===========================================================================*/

static void exe_process_handle_create(const exec_request_t *req, exec_response_t *resp) {
    proc_create_req_t *payload = (proc_create_req_t *)req->payload;
    
    /* Determine target address based on module index */
    /* TODO: Better memory allocation */
    uint32_t target_addr = 0x00400000 + (payload->module_index * 0x100000);
    
    int pid = exe_process_create_from_module(payload->module_index, target_addr);
    
    resp->msg_id = req->msg_id;
    resp->status = (pid >= 0) ? EXEC_OK : pid;
    resp->result = (pid >= 0) ? (uint32_t)pid : 0;
    resp->payload_size = 0;
}

static void exe_process_handle_terminate(const exec_request_t *req, exec_response_t *resp) {
    proc_terminate_req_t *payload = (proc_terminate_req_t *)req->payload;
    
    int result = exe_proc_terminate(payload->pid);
    
    resp->msg_id = req->msg_id;
    resp->status = (result >= 0) ? EXEC_OK : result;
    resp->result = 0;
    resp->payload_size = 0;
}

static void exe_process_handle_get_pid(const exec_request_t *req, exec_response_t *resp) {
    int pid = exe_proc_get_pid();
    
    resp->msg_id = req->msg_id;
    resp->status = (pid >= 0) ? EXEC_OK : pid;
    resp->result = (pid >= 0) ? (uint32_t)pid : 0;
    resp->payload_size = 0;
}

static void exe_process_handle_ping(const exec_request_t *req, exec_response_t *resp) {
    resp->msg_id = req->msg_id;
    resp->status = EXEC_OK;
    resp->result = 1;
    resp->payload_size = 0;
}

/*=============================================================================
 * REQUEST DISPATCHER
 *===========================================================================*/

static void exe_process_process_request(const exec_request_t *req, exec_response_t *resp) {
    exe_memset(resp, 0, sizeof(exec_response_t));
    
    switch (req->opcode) {
        case EXEC_OP_PING:
            exe_process_handle_ping(req, resp);
            break;
            
        case EXEC_OP_SHUTDOWN:
            g_ecb->stop_requested = 1;
            resp->msg_id = req->msg_id;
            resp->status = EXEC_OK;
            break;
            
        case PROC_OP_CREATE:
            exe_process_handle_create(req, resp);
            break;
            
        case PROC_OP_TERMINATE:
            exe_process_handle_terminate(req, resp);
            break;
            
        case PROC_OP_GET_PID:
            exe_process_handle_get_pid(req, resp);
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

static int exe_process_init(void) {
    exe_klog(LOG_INFO, "PROCEXEC", "Process Executive initializing...");
    
    g_req_queue_shm_id = exe_shm_create(sizeof(exec_request_queue_t));
    if (g_req_queue_shm_id < 0) {
        exe_klog(LOG_ERROR, "PROCEXEC", "Failed to create request queue SHM");
        return -1;
    }
    
    g_req_queue = (exec_request_queue_t *)exe_shm_attach(g_req_queue_shm_id);
    if (!g_req_queue) {
        exe_klog(LOG_ERROR, "PROCEXEC", "Failed to attach request queue SHM");
        return -1;
    }
    exe_request_queue_init(g_req_queue);
    exe_klog_hex(LOG_INFO, "PROCEXEC", "Request queue SHM ID:", g_req_queue_shm_id);
    
    g_resp_queue_shm_id = exe_shm_create(sizeof(exec_response_queue_t));
    if (g_resp_queue_shm_id < 0) {
        exe_klog(LOG_ERROR, "PROCEXEC", "Failed to create response queue SHM");
        return -1;
    }
    
    g_resp_queue = (exec_response_queue_t *)exe_shm_attach(g_resp_queue_shm_id);
    if (!g_resp_queue) {
        exe_klog(LOG_ERROR, "PROCEXEC", "Failed to attach response queue SHM");
        return -1;
    }
    exe_response_queue_init(g_resp_queue);
    exe_klog_hex(LOG_INFO, "PROCEXEC", "Response queue SHM ID:", g_resp_queue_shm_id);
    
    static exec_control_block_t local_ecb;
    g_ecb = &local_ecb;
    exe_str_copy(g_ecb->name, "process_executive", EXEC_NAME_MAX);
    g_ecb->exec_id = EXEC_ID_PROCESS;
    g_ecb->state = EXEC_STATE_STARTING;
    g_ecb->priority = PRIORITY_HIGH;
    g_ecb->request_queue_shm_id = g_req_queue_shm_id;
    g_ecb->response_queue_shm_id = g_resp_queue_shm_id;
    
    /* Publish SHM IDs to cells for discovery */
    exe_cell_write("system.exec.process.req_shm", &g_req_queue_shm_id, sizeof(int));
    exe_cell_write("system.exec.process.resp_shm", &g_resp_queue_shm_id, sizeof(int));
    
    exe_klog(LOG_INFO, "PROCEXEC", "Process Executive initialized successfully");
    return 0;
}

void exe_process_main(void) {
    if (exe_process_init() != 0) {
        exe_klog(LOG_ERROR, "PROCEXEC", "Initialization failed! Halting.");
        while (1) __asm__ volatile("hlt");
    }
    
    EXEC_SET_STATE(g_ecb, EXEC_STATE_RUNNING);
    exe_klog(LOG_INFO, "PROCEXEC", "Entering main loop...");
    
    exec_request_t req;
    exec_response_t resp;
    
    while (!EXEC_SHOULD_STOP(g_ecb)) {
        if (exe_request_queue_pop(g_req_queue, &req) == EXEC_OK) {
            exe_process_process_request(&req, &resp);
            exe_response_queue_push(g_resp_queue, &resp);
        } else {
            exe_yield();
        }
    }
    
    EXEC_SET_STATE(g_ecb, EXEC_STATE_STOPPED);
    exe_klog(LOG_WARN, "PROCEXEC", "Stopped. Halting.");
    while (1) __asm__ volatile("hlt");
}
