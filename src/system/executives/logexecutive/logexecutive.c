/**
 * MaahiOS Log Executive Implementation
 * 
 * Description:
 *   Log Executive receives log requests from user-space apps via SHM queue,
 *   then calls the ulog syscall which outputs with [U] prefix.
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#include "log_executive.h"
#include "../common/executive_queue.h"

/*=============================================================================
 * SYSCALL NUMBERS
 *===========================================================================*/

#define SYS_YIELD           1
#define SYS_SHM_CREATE      48
#define SYS_SHM_ATTACH      49
#define SYS_CELL_WRITE      64
#define SYS_CELL_READ       65
#define SYS_KLOG            240  /* Outputs [U] when called via syscall from ring 3 */
#define SYS_KLOG_HEX        241

/* Log levels */
#define LOG_INFO    3
#define LOG_WARN    2
#define LOG_ERROR   1

/* Cell types and flags */
#define CELL_TYPE_INT       1
#define CELL_FLAG_SYSTEM    (1 << 2)

/*=============================================================================
 * SYSCALL WRAPPERS
 * NOTE: The kernel syscall handler does NOT preserve ECX or EDX.
 * Only EBX, ESI, EDI, EBP are callee-saved. EAX is the return value.
 *===========================================================================*/

static inline int syscall0(int num) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num) : "memory", "ecx", "edx");
    return ret;
}

static inline int syscall1(int num, int a1) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(a1) : "memory", "ecx", "edx");
    return ret;
}

static inline int syscall2(int num, int a1, int a2) {
    int ret;
    int _ecx;
    __asm__ volatile("int $0x80"
        : "=a"(ret), "=c"(_ecx)
        : "a"(num), "b"(a1), "1"(a2)
        : "memory", "edx");
    return ret;
}

static inline int syscall3(int num, int a1, int a2, int a3) {
    int ret;
    int _ecx, _edx;
    __asm__ volatile("int $0x80"
        : "=a"(ret), "=c"(_ecx), "=d"(_edx)
        : "a"(num), "b"(a1), "1"(a2), "2"(a3)
        : "memory");
    return ret;
}

static inline int syscall4(int num, int a1, int a2, int a3, int a4) {
    int ret;
    int _ecx, _edx;
    __asm__ volatile("int $0x80"
        : "=a"(ret), "=c"(_ecx), "=d"(_edx)
        : "a"(num), "b"(a1), "1"(a2), "2"(a3), "S"(a4)
        : "memory");
    return ret;
}

static inline void exe_yield(void) {
    syscall0(SYS_YIELD);
}

static inline int exe_shm_create(uint32_t size) {
    return syscall1(SYS_SHM_CREATE, (int)size);
}

static inline void* exe_shm_attach(int shm_id) {
    return (void*)syscall2(SYS_SHM_ATTACH, shm_id, 0);
}

/* cell_write auto-creates if key doesn't exist, so no need for register */
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

static void exe_log_handle_log(const exec_request_t *req, exec_response_t *resp) {
    log_entry_req_t *payload = (log_entry_req_t *)req->payload;
    
    /* Call klog syscall - outputs with [U] prefix since we're ring 3 */
    exe_klog(payload->level, payload->tag, payload->msg);
    
    resp->msg_id = req->msg_id;
    resp->status = EXEC_OK;
    resp->result = 0;
    resp->payload_size = 0;
}

static void exe_log_handle_log_hex(const exec_request_t *req, exec_response_t *resp) {
    log_hex_req_t *payload = (log_hex_req_t *)req->payload;
    
    /* Call klog_hex syscall - outputs with [U] prefix since we're ring 3 */
    exe_klog_hex(payload->level, payload->tag, payload->msg, payload->value);
    
    resp->msg_id = req->msg_id;
    resp->status = EXEC_OK;
    resp->result = 0;
    resp->payload_size = 0;
}

/*=============================================================================
 * REQUEST DISPATCHER
 *===========================================================================*/

static void exe_log_process_request(const exec_request_t *req, exec_response_t *resp) {
    exe_memset(resp, 0, sizeof(exec_response_t));
    
    switch (req->func_id) {
        case LOG_OP_LOG:
            exe_log_handle_log(req, resp);
            break;
            
        case LOG_OP_LOG_HEX:
            exe_log_handle_log_hex(req, resp);
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
 * INITIALIZATION
 *===========================================================================*/

static int exe_log_init(void) {
    exe_klog(LOG_INFO, "LOGEXEC", "Log Executive initializing...");
    
    /* Create our request queue */
    g_req_queue_shm_id = exe_shm_create(sizeof(exec_request_queue_t));
    if (g_req_queue_shm_id < 0) {
        exe_klog(LOG_ERROR, "LOGEXEC", "Failed to create request queue SHM");
        return -1;
    }
    
    g_req_queue = (exec_request_queue_t *)exe_shm_attach(g_req_queue_shm_id);
    if (!g_req_queue) {
        exe_klog(LOG_ERROR, "LOGEXEC", "Failed to attach request queue SHM");
        return -1;
    }
    exe_request_queue_init(g_req_queue);
    exe_klog_hex(LOG_INFO, "LOGEXEC", "Request queue SHM ID:", g_req_queue_shm_id);
    
    /* Create our response queue */
    g_resp_queue_shm_id = exe_shm_create(sizeof(exec_response_queue_t));
    if (g_resp_queue_shm_id < 0) {
        exe_klog(LOG_ERROR, "LOGEXEC", "Failed to create response queue SHM");
        return -1;
    }
    
    g_resp_queue = (exec_response_queue_t *)exe_shm_attach(g_resp_queue_shm_id);
    if (!g_resp_queue) {
        exe_klog(LOG_ERROR, "LOGEXEC", "Failed to attach response queue SHM");
        return -1;
    }
    exe_response_queue_init(g_resp_queue);
    exe_klog_hex(LOG_INFO, "LOGEXEC", "Response queue SHM ID:", g_resp_queue_shm_id);
    
    /* Setup ECB */
    static exec_control_block_t local_ecb;
    g_ecb = &local_ecb;
    exe_str_copy(g_ecb->name, "log_executive", EXEC_NAME_MAX);
    g_ecb->exec_id = EXEC_ID_LOG;
    g_ecb->state = EXEC_STATE_STARTING;
    g_ecb->priority = PRIORITY_HIGH;
    g_ecb->request_queue_shm_id = g_req_queue_shm_id;
    g_ecb->response_queue_shm_id = g_resp_queue_shm_id;
    
    /* Write our queue SHM IDs to cells so clients can find us */
    /* cell_write auto-creates the key if it doesn't exist */
    exe_cell_write("system.exec.log.req_shm", &g_req_queue_shm_id, sizeof(int));
    exe_cell_write("system.exec.log.resp_shm", &g_resp_queue_shm_id, sizeof(int));
    
    exe_klog(LOG_INFO, "LOGEXEC", "Log Executive initialized successfully");
    return 0;
}

/*=============================================================================
 * MAIN LOOP
 *===========================================================================*/

void exe_log_main(void) {
    if (exe_log_init() != 0) {
        exe_klog(LOG_ERROR, "LOGEXEC", "Initialization failed! Idling.");
        while (1) exe_yield();  /* Init failed, idle forever */
    }
    
    EXEC_SET_STATE(g_ecb, EXEC_STATE_RUNNING);
    exe_klog(LOG_INFO, "LOGEXEC", "Entering main loop...");
    
    exec_request_t req;
    exec_response_t resp;
    
    while (!EXEC_SHOULD_STOP(g_ecb)) {
        if (exe_request_queue_pop(g_req_queue, &req) == EXEC_OK) {
            exe_log_process_request(&req, &resp);
            exe_response_queue_push(g_resp_queue, &resp);
        } else {
            exe_yield();
        }
    }
    
    EXEC_SET_STATE(g_ecb, EXEC_STATE_STOPPED);
    exe_klog(LOG_WARN, "LOGEXEC", "Stopped. Idling.");
    while (1) exe_yield();
}
