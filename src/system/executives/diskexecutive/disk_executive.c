/**
 * MaahiOS Disk Executive Implementation
 * 
 * Description:
 *   Disk Executive provides storage access services.
 *   Makes syscalls to kernel device_manager for disk operations.
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#include "disk_executive.h"
#include "../common/executive_queue.h"

/*=============================================================================
 * SYSCALL INTERFACE
 *===========================================================================*/

#define SYS_YIELD           1
#define SYS_SHM_CREATE      48
#define SYS_SHM_ATTACH      49
#define SYS_CELL_WRITE      64
#define SYS_DEV_OPEN        80
#define SYS_DEV_CLOSE       81
#define SYS_DEV_READ        82
#define SYS_DEV_WRITE       83
#define SYS_DEV_IOCTL       84
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

static inline int exe_dev_read(int fd, void *buf, uint32_t size) {
    return syscall3(SYS_DEV_READ, fd, (int)buf, (int)size);
}

static inline int exe_dev_ioctl(int fd, uint32_t cmd, void *arg) {
    return syscall3(SYS_DEV_IOCTL, fd, (int)cmd, (int)arg);
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
 * DISK OPERATIONS (via device manager syscalls)
 *===========================================================================*/

static int exe_disk_read_sector(uint32_t disk_id, uint32_t sector, void *buffer, uint32_t count) {
    /* TODO: Implement via device manager syscalls */
    /* For now, return not implemented */
    (void)disk_id;
    (void)sector;
    (void)buffer;
    (void)count;
    return EXEC_ERR_NOT_FOUND;
}

static int exe_disk_read_file(const char *path, uint32_t offset, void *buffer, uint32_t size) {
    /* TODO: Implement via device manager + ISO9660 syscalls */
    (void)path;
    (void)offset;
    (void)buffer;
    (void)size;
    return EXEC_ERR_NOT_FOUND;
}

/*=============================================================================
 * REQUEST HANDLERS
 *===========================================================================*/

static void exe_disk_handle_read_sector(const exec_request_t *req, exec_response_t *resp) {
    disk_read_sector_req_t *payload = (disk_read_sector_req_t *)req->payload;
    disk_read_resp_t *resp_data = (disk_read_resp_t *)resp->payload;
    
    int result = exe_disk_read_sector(payload->disk_id, payload->sector,
                                       resp_data->data, payload->count);
    
    resp->msg_id = req->msg_id;
    resp->status = (result >= 0) ? EXEC_OK : result;
    resp->result = 0;
    
    if (result >= 0) {
        resp_data->bytes_read = (uint32_t)result;
        resp->payload_size = sizeof(uint32_t) + result;
    } else {
        resp->payload_size = 0;
    }
}

static void exe_disk_handle_read_file(const exec_request_t *req, exec_response_t *resp) {
    disk_read_file_req_t *payload = (disk_read_file_req_t *)req->payload;
    disk_read_resp_t *resp_data = (disk_read_resp_t *)resp->payload;
    
    uint32_t read_size = (payload->size > DISK_MAX_READ) ? DISK_MAX_READ : payload->size;
    int result = exe_disk_read_file(payload->path, payload->offset,
                                     resp_data->data, read_size);
    
    resp->msg_id = req->msg_id;
    resp->status = (result >= 0) ? EXEC_OK : result;
    resp->result = 0;
    
    if (result >= 0) {
        resp_data->bytes_read = (uint32_t)result;
        resp->payload_size = sizeof(uint32_t) + result;
    } else {
        resp->payload_size = 0;
    }
}

static void exe_disk_handle_ping(const exec_request_t *req, exec_response_t *resp) {
    resp->msg_id = req->msg_id;
    resp->status = EXEC_OK;
    resp->result = 1;
    resp->payload_size = 0;
}

/*=============================================================================
 * REQUEST DISPATCHER
 *===========================================================================*/

static void exe_disk_process_request(const exec_request_t *req, exec_response_t *resp) {
    exe_memset(resp, 0, sizeof(exec_response_t));
    
    switch (req->opcode) {
        case EXEC_OP_PING:
            exe_disk_handle_ping(req, resp);
            break;
            
        case EXEC_OP_SHUTDOWN:
            g_ecb->stop_requested = 1;
            resp->msg_id = req->msg_id;
            resp->status = EXEC_OK;
            break;
            
        case DISK_OP_READ_SECTOR:
            exe_disk_handle_read_sector(req, resp);
            break;
            
        case DISK_OP_READ_FILE:
            exe_disk_handle_read_file(req, resp);
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

static int exe_disk_init(void) {
    exe_klog(LOG_INFO, "DISKEXEC", "Disk Executive initializing...");
    
    g_req_queue_shm_id = exe_shm_create(sizeof(exec_request_queue_t));
    if (g_req_queue_shm_id < 0) {
        exe_klog(LOG_ERROR, "DISKEXEC", "Failed to create request queue SHM");
        return -1;
    }
    
    g_req_queue = (exec_request_queue_t *)exe_shm_attach(g_req_queue_shm_id);
    if (!g_req_queue) {
        exe_klog(LOG_ERROR, "DISKEXEC", "Failed to attach request queue SHM");
        return -1;
    }
    exe_request_queue_init(g_req_queue);
    exe_klog_hex(LOG_INFO, "DISKEXEC", "Request queue SHM ID:", g_req_queue_shm_id);
    
    g_resp_queue_shm_id = exe_shm_create(sizeof(exec_response_queue_t));
    if (g_resp_queue_shm_id < 0) {
        exe_klog(LOG_ERROR, "DISKEXEC", "Failed to create response queue SHM");
        return -1;
    }
    
    g_resp_queue = (exec_response_queue_t *)exe_shm_attach(g_resp_queue_shm_id);
    if (!g_resp_queue) {
        exe_klog(LOG_ERROR, "DISKEXEC", "Failed to attach response queue SHM");
        return -1;
    }
    exe_response_queue_init(g_resp_queue);
    exe_klog_hex(LOG_INFO, "DISKEXEC", "Response queue SHM ID:", g_resp_queue_shm_id);
    
    static exec_control_block_t local_ecb;
    g_ecb = &local_ecb;
    exe_str_copy(g_ecb->name, "disk_executive", EXEC_NAME_MAX);
    g_ecb->exec_id = EXEC_ID_DISK;
    g_ecb->state = EXEC_STATE_STARTING;
    g_ecb->priority = PRIORITY_HIGH;
    g_ecb->request_queue_shm_id = g_req_queue_shm_id;
    g_ecb->response_queue_shm_id = g_resp_queue_shm_id;
    
    /* Publish SHM IDs to cells for discovery */
    exe_cell_write("system.exec.disk.req_shm", &g_req_queue_shm_id, sizeof(int));
    exe_cell_write("system.exec.disk.resp_shm", &g_resp_queue_shm_id, sizeof(int));
    
    exe_klog(LOG_INFO, "DISKEXEC", "Disk Executive initialized successfully");
    return 0;
}

void exe_disk_main(void) {
    if (exe_disk_init() != 0) {
        exe_klog(LOG_ERROR, "DISKEXEC", "Initialization failed! Halting.");
        while (1) __asm__ volatile("hlt");
    }
    
    EXEC_SET_STATE(g_ecb, EXEC_STATE_RUNNING);
    exe_klog(LOG_INFO, "DISKEXEC", "Entering main loop...");
    
    exec_request_t req;
    exec_response_t resp;
    
    while (!EXEC_SHOULD_STOP(g_ecb)) {
        if (exe_request_queue_pop(g_req_queue, &req) == EXEC_OK) {
            exe_disk_process_request(&req, &resp);
            exe_response_queue_push(g_resp_queue, &resp);
        } else {
            exe_yield();
        }
    }
    
    EXEC_SET_STATE(g_ecb, EXEC_STATE_STOPPED);
    exe_klog(LOG_WARN, "DISKEXEC", "Stopped. Halting.");
    while (1) __asm__ volatile("hlt");
}
