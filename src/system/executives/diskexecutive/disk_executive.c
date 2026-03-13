/**
 * MaahiOS Disk Executive Implementation
 * 
 * Description:
 *   Block-level storage executive. Enumerates disks, reads/writes sectors.
 *   Talks to kernel Device Manager via SYS_DEV_* syscalls.
 *   Does NOT handle filesystems (that's a future FS Executive).
 * 
 *   PID 7 - loaded 5th by sysman (after Log, Cell, Process, Memory)
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#include "disk_executive.h"
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
 * DEVICE MANAGER SYSCALL WRAPPERS
 *===========================================================================*/

static inline int exe_dev_open(int device_id, int flags) {
    return syscall2(SYS_DEV_OPEN, device_id, flags);
}

static inline int exe_dev_close(int device_id, int handle) {
    return syscall2(SYS_DEV_CLOSE, device_id, handle);
}

static inline int exe_dev_read(int device_id, void *buf, uint32_t size) {
    return syscall3(SYS_DEV_READ, device_id, (int)buf, (int)size);
}

static inline int exe_dev_write(int device_id, const void *buf, uint32_t size) {
    return syscall3(SYS_DEV_WRITE, device_id, (int)buf, (int)size);
}

static inline int exe_dev_ioctl(int device_id, int cmd, void *arg) {
    return syscall3(SYS_DEV_IOCTL, device_id, cmd, (int)arg);
}

static inline int exe_dev_poll(int device_id) {
    return syscall1(SYS_DEV_POLL, device_id);
}

/*=============================================================================
 * DEVICE MANAGER CONSTANTS (must match device_manager.h)
 *===========================================================================*/

#define DEV_DISK            4
#define DISK_IOCTL_GET_INFO         1
#define DISK_IOCTL_GET_SECTOR_SIZE  2
#define DISK_IOCTL_GET_SECTOR_COUNT 3
#define DISK_IOCTL_FLUSH            4

/*=============================================================================
 * DISK SUBSYSTEM INFO (matches disk_subsystem.h disk_info_t exactly)
 * We duplicate it here since we're Ring 3 and can't include kernel headers.
 *===========================================================================*/

typedef struct {
    uint8_t  active;
    uint8_t  disk_type;
    uint8_t  fs_type;
    uint8_t  _reserved;     /* Was drive_letter; letters are volume-only now */
    uint8_t  ata_drive_id;
    uint32_t size_mb;
    char     label[32];
    char     type_str[16];
    char     fs_str[16];
} kernel_disk_info_t;

/*=============================================================================
 * EXECUTIVE STATE
 *===========================================================================*/

static exec_control_block_t  *g_ecb = NULL;
static exec_request_queue_t  *g_req_queue = NULL;
static exec_response_queue_t *g_resp_queue = NULL;
static int g_req_queue_shm_id  = -1;
static int g_resp_queue_shm_id = -1;

/* Cached disk info (populated at init and on LIST_DISKS) */
static disk_exec_info_t g_disk_cache[DISK_MAX_DISKS];
static int g_disk_count = 0;
static int g_dev_handle = -1;   /* Device handle from dev_open */

/*=============================================================================
 * INTERNAL: SCAN DISKS VIA DEVICE MANAGER
 *
 * Strategy: Open DEV_DISK, use dev_read to get first disk info,
 * and use ioctl GET_INFO with index to enumerate.
 *===========================================================================*/

static void exe_disk_scan(void) {
    g_disk_count = 0;

    /* Read disk count from cell (published by kernel disk.c at boot) */
    int count = 0;
    int result = libcell_read("device.disk.count", &count, sizeof(int));
    if (result < 0 || count <= 0) {
        liblog(LOG_WARN, "DISKEXEC", "No disks found in cell registry");
        return;
    }
    if (count > DISK_MAX_DISKS) count = DISK_MAX_DISKS;

    /* Read per-disk info from cells */
    for (int i = 0; i < count; i++) {
        char key[32] = "device.disk.0";
        key[12] = '0' + (char)i;

        kernel_disk_info_t kdi;
        exe_memset(&kdi, 0, sizeof(kdi));
        result = libcell_read(key, &kdi, sizeof(kernel_disk_info_t));
        if (result < 0 || !kdi.active) continue;

        disk_exec_info_t *d = &g_disk_cache[g_disk_count];
        d->index      = (uint8_t)i;
        d->disk_type  = kdi.disk_type;
        d->status     = DISK_STATUS_ONLINE;
        d->reserved   = 0;
        d->size_mb    = kdi.size_mb;

        /* Sector size: CDROM=2048, HDD=512 */
        if (kdi.disk_type == DISK_TYPE_CDROM) {
            d->sector_size = 2048;
        } else {
            d->sector_size = 512;
        }

        /* Build name from type_str and drive letter */
        exe_str_copy(d->name, kdi.type_str, DISK_NAME_MAX);

        g_disk_count++;

        liblog_hex(LOG_INFO, "DISKEXEC", "Found disk index:", (uint32_t)i);
        liblog(LOG_INFO, "DISKEXEC", kdi.type_str);
        liblog_hex(LOG_INFO, "DISKEXEC", "  Size MB:", kdi.size_mb);
    }

    liblog_hex(LOG_INFO, "DISKEXEC", "Total disks found:", g_disk_count);
}

/*=============================================================================
 * REQUEST HANDLERS
 *===========================================================================*/

static void exe_disk_handle_ping(const exec_request_t *req, exec_response_t *resp) {
    resp->msg_id = req->msg_id;
    resp->status = EXEC_OK;
    resp->result = 1;
    resp->payload_size = 0;
}

static void exe_disk_handle_list_disks(const exec_request_t *req, exec_response_t *resp) {
    /* Return cached info (populated at init from cells — no re-scan needed) */
    disk_list_resp_t *list = (disk_list_resp_t *)resp->payload;
    
    /* Copy cached info to response (max 5 due to payload size limit) */
    int count = (g_disk_count > 5) ? 5 : g_disk_count;
    list->count = (uint32_t)count;
    
    for (int i = 0; i < count; i++) {
        list->disks[i] = g_disk_cache[i];
    }
    
    resp->msg_id = req->msg_id;
    resp->status = EXEC_OK;
    resp->result = (uint32_t)g_disk_count;
    resp->payload_size = sizeof(uint32_t) + (count * sizeof(disk_exec_info_t));
}

static void exe_disk_handle_get_info(const exec_request_t *req, exec_response_t *resp) {
    disk_get_info_req_t *payload = (disk_get_info_req_t *)req->payload;
    uint8_t idx = payload->disk_index;
    
    if (idx >= g_disk_count) {
        resp->msg_id = req->msg_id;
        resp->status = EXEC_ERR_NOT_FOUND;
        resp->payload_size = 0;
        return;
    }
    
    disk_info_resp_t *info_resp = (disk_info_resp_t *)resp->payload;
    info_resp->info = g_disk_cache[idx];
    
    resp->msg_id = req->msg_id;
    resp->status = EXEC_OK;
    resp->result = 0;
    resp->payload_size = sizeof(disk_info_resp_t);
}

static void exe_disk_handle_get_status(const exec_request_t *req, exec_response_t *resp) {
    disk_get_status_req_t *payload = (disk_get_status_req_t *)req->payload;
    uint8_t idx = payload->disk_index;
    
    if (idx >= g_disk_count) {
        resp->msg_id = req->msg_id;
        resp->status = EXEC_ERR_NOT_FOUND;
        resp->result = DISK_STATUS_OFFLINE;
        resp->payload_size = 0;
        return;
    }
    
    /* Check if device is still responding */
    int poll_result = exe_dev_poll(DEV_DISK);
    
    resp->msg_id = req->msg_id;
    resp->status = EXEC_OK;
    resp->result = (poll_result > 0) ? DISK_STATUS_ONLINE : DISK_STATUS_ERROR;
    resp->payload_size = 0;
    
    /* Update cached status */
    g_disk_cache[idx].status = (uint8_t)resp->result;
}

static void exe_disk_handle_read_sector(const exec_request_t *req, exec_response_t *resp) {
    disk_read_sector_req_t *payload = (disk_read_sector_req_t *)req->payload;
    uint8_t idx = payload->disk_index;
    
    if (idx >= g_disk_count) {
        resp->msg_id = req->msg_id;
        resp->status = EXEC_ERR_NOT_FOUND;
        resp->payload_size = 0;
        return;
    }
    
    /* Determine sector size for this disk */
    uint32_t sect_size = g_disk_cache[idx].sector_size;
    if (sect_size == 0) sect_size = 512;
    
    /* Create a SHM block for the sector data */
    int data_shm_id = exe_shm_create(sect_size);
    if (data_shm_id < 0) {
        liblog(LOG_ERROR, "DISKEXEC", "Failed to create SHM for sector data");
        resp->msg_id = req->msg_id;
        resp->status = EXEC_ERR_NO_MEMORY;
        resp->payload_size = 0;
        return;
    }
    
    void *data_buf = exe_shm_attach(data_shm_id);
    if (!data_buf) {
        liblog(LOG_ERROR, "DISKEXEC", "Failed to attach SHM for sector data");
        syscall1(SYS_SHM_DESTROY, data_shm_id);
        resp->msg_id = req->msg_id;
        resp->status = EXEC_ERR_NO_MEMORY;
        resp->payload_size = 0;
        return;
    }
    
    /* Read sector via device manager
     * The disk_subsystem read_sector operates on ATA drive IDs internally.
     * From Ring 3, we use SYS_DEV_READ which goes through device_manager.
     *
     * However, the current disk_subsystem dev_read only returns the first
     * disk's info. For proper sector reads, we'd need an ioctl or a more
     * sophisticated dev_read that accepts LBA parameters.
     *
     * For now, we use SYS_DEV_READ with the data buffer.
     * TODO: Add a DISK_IOCTL_READ_SECTOR ioctl to disk_subsystem for
     *       LBA-addressed reads from Ring 3. */
    int read_result = exe_dev_read(DEV_DISK, data_buf, sect_size);
    
    if (read_result < 0) {
        liblog_hex(LOG_ERROR, "DISKEXEC", "Sector read failed, err:", (uint32_t)read_result);
        syscall1(SYS_SHM_DETACH, data_shm_id);
        syscall1(SYS_SHM_DESTROY, data_shm_id);
        resp->msg_id = req->msg_id;
        resp->status = read_result;
        resp->payload_size = 0;
        return;
    }
    
    /* Detach our side — caller will attach, copy, detach, destroy */
    syscall1(SYS_SHM_DETACH, data_shm_id);
    liblog_hex(LOG_DEBUG, "DISKEXEC", "Sector read OK, SHM ID:", (uint32_t)data_shm_id);
    
    resp->msg_id = req->msg_id;
    resp->status = EXEC_OK;
    resp->result = (uint32_t)data_shm_id;  /* Caller uses this SHM ID to get data */
    resp->payload_size = 0;
}

static void exe_disk_handle_get_sector_size(const exec_request_t *req, exec_response_t *resp) {
    disk_get_sector_size_req_t *payload = (disk_get_sector_size_req_t *)req->payload;
    uint8_t idx = payload->disk_index;
    
    if (idx >= g_disk_count) {
        resp->msg_id = req->msg_id;
        resp->status = EXEC_ERR_NOT_FOUND;
        resp->payload_size = 0;
        return;
    }
    
    resp->msg_id = req->msg_id;
    resp->status = EXEC_OK;
    resp->result = g_disk_cache[idx].sector_size;
    resp->payload_size = 0;
}

static void exe_disk_handle_format(const exec_request_t *req, exec_response_t *resp) {
    disk_format_req_t *payload = (disk_format_req_t *)req->payload;
    uint8_t idx = payload->disk_index;

    if (idx >= g_disk_count) {
        resp->msg_id = req->msg_id;
        resp->status = EXEC_ERR_NOT_FOUND;
        resp->payload_size = 0;
        return;
    }

    /* Only HDDs can be formatted */
    if (g_disk_cache[idx].disk_type == DISK_TYPE_CDROM) {
        liblog(LOG_WARN, "DISKEXEC", "Cannot format CD-ROM");
        resp->msg_id = req->msg_id;
        resp->status = EXEC_ERR_INVALID;
        resp->payload_size = 0;
        return;
    }

    liblog(LOG_INFO, "DISKEXEC", "Format request for disk");
    liblog_hex(LOG_INFO, "DISKEXEC", "  disk index:", (uint32_t)idx);

    /* Call SYS_DISK_FORMAT syscall → kernel orchestrates MBR + MFS + mount */
    int result = syscall2(SYS_DISK_FORMAT, (int)idx, (int)payload->label);

    resp->msg_id = req->msg_id;
    resp->status = (result == 0) ? EXEC_OK : result;
    resp->result = (uint32_t)result;
    resp->payload_size = 0;

    if (result == 0) {
        liblog(LOG_INFO, "DISKEXEC", "Format completed successfully");
        /* Re-scan disks to update cache with new MFS info */
        exe_disk_scan();
    } else {
        liblog_hex(LOG_ERROR, "DISKEXEC", "Format failed, err:", (uint32_t)result);
    }
}

/*=============================================================================
 * REQUEST DISPATCHER
 *===========================================================================*/

static void exe_disk_dispatch(const exec_request_t *req, exec_response_t *resp) {
    exe_memset(resp, 0, sizeof(exec_response_t));
    
    switch (req->func_id) {
        case EXEC_OP_PING:
            exe_disk_handle_ping(req, resp);
            break;
            
        case EXEC_OP_SHUTDOWN:
            g_ecb->stop_requested = 1;
            resp->msg_id = req->msg_id;
            resp->status = EXEC_OK;
            break;
            
        case DISK_OP_LIST_DISKS:
            exe_disk_handle_list_disks(req, resp);
            break;
            
        case DISK_OP_GET_INFO:
            exe_disk_handle_get_info(req, resp);
            break;
            
        case DISK_OP_GET_STATUS:
            exe_disk_handle_get_status(req, resp);
            break;
            
        case DISK_OP_READ_SECTOR:
            exe_disk_handle_read_sector(req, resp);
            break;
            
        case DISK_OP_WRITE_SECTOR:
            /* Future: write sector support */
            resp->msg_id = req->msg_id;
            resp->status = EXEC_ERR_INVALID;
            resp->payload_size = 0;
            break;
            
        case DISK_OP_GET_SECTOR_SIZE:
            exe_disk_handle_get_sector_size(req, resp);
            break;

        case DISK_OP_FORMAT:
            exe_disk_handle_format(req, resp);
            break;
            
        default:
            resp->msg_id = req->msg_id;
            resp->status = EXEC_ERR_INVALID;
            resp->payload_size = 0;
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
    liblog(LOG_INFO, "DISKEXEC", "Disk Executive initializing...");
    
    /* Create SHM queues */
    g_req_queue_shm_id = exe_shm_create(sizeof(exec_request_queue_t));
    if (g_req_queue_shm_id < 0) {
        liblog(LOG_ERROR, "DISKEXEC", "Failed to create request queue SHM");
        return -1;
    }
    
    g_req_queue = (exec_request_queue_t *)exe_shm_attach(g_req_queue_shm_id);
    if (!g_req_queue) {
        liblog(LOG_ERROR, "DISKEXEC", "Failed to attach request queue SHM");
        return -1;
    }
    exe_request_queue_init(g_req_queue);
    liblog_hex(LOG_INFO, "DISKEXEC", "Request queue SHM ID:", g_req_queue_shm_id);
    
    g_resp_queue_shm_id = exe_shm_create(sizeof(exec_response_queue_t));
    if (g_resp_queue_shm_id < 0) {
        liblog(LOG_ERROR, "DISKEXEC", "Failed to create response queue SHM");
        return -1;
    }
    
    g_resp_queue = (exec_response_queue_t *)exe_shm_attach(g_resp_queue_shm_id);
    if (!g_resp_queue) {
        liblog(LOG_ERROR, "DISKEXEC", "Failed to attach response queue SHM");
        return -1;
    }
    exe_response_queue_init(g_resp_queue);
    liblog_hex(LOG_INFO, "DISKEXEC", "Response queue SHM ID:", g_resp_queue_shm_id);
    
    /* Set up control block */
    static exec_control_block_t local_ecb;
    g_ecb = &local_ecb;
    exe_str_copy(g_ecb->name, "disk_executive", EXEC_NAME_MAX);
    g_ecb->exec_id = EXEC_ID_DISK;
    g_ecb->state = EXEC_STATE_STARTING;
    g_ecb->priority = PRIORITY_HIGH;
    g_ecb->request_queue_shm_id = g_req_queue_shm_id;
    g_ecb->response_queue_shm_id = g_resp_queue_shm_id;
    
    /* Write queue SHM IDs to cells for discovery */
    libcell_write("system.exec.disk.req_shm", &g_req_queue_shm_id, sizeof(int));
    libcell_write("system.exec.disk.resp_shm", &g_resp_queue_shm_id, sizeof(int));
    
    /* Open disk device */
    g_dev_handle = exe_dev_open(DEV_DISK, 0);
    liblog_hex(LOG_INFO, "DISKEXEC", "DEV_DISK open handle:", (uint32_t)g_dev_handle);
    
    /* Initial disk scan */
    exe_disk_scan();
    
    /* Publish disk count to cell registry */
    int count = g_disk_count;
    libcell_write("system.disk.count", &count, sizeof(int));
    
    /* Publish per-disk info to cells */
    for (int i = 0; i < g_disk_count; i++) {
        /* system.disk.N.type */
        char key[48];
        /* Build key manually since we have no snprintf */
        /* system.disk.0.type, system.disk.0.size_mb, etc. */
        key[0] = 's'; key[1] = 'y'; key[2] = 's'; key[3] = 't';
        key[4] = 'e'; key[5] = 'm'; key[6] = '.'; key[7] = 'd';
        key[8] = 'i'; key[9] = 's'; key[10] = 'k'; key[11] = '.';
        key[12] = '0' + (char)i; key[13] = '.';
        
        /* Publish type */
        key[14] = 't'; key[15] = 'y'; key[16] = 'p'; key[17] = 'e'; key[18] = '\0';
        int dtype = g_disk_cache[i].disk_type;
        libcell_write(key, &dtype, sizeof(int));
        
        /* Publish size_mb */
        key[14] = 's'; key[15] = 'i'; key[16] = 'z'; key[17] = 'e'; key[18] = '\0';
        int sz = (int)g_disk_cache[i].size_mb;
        libcell_write(key, &sz, sizeof(int));
        
        /* Publish status */
        key[14] = 's'; key[15] = 't'; key[16] = 'a'; key[17] = 't'; key[18] = '\0';
        int st = g_disk_cache[i].status;
        libcell_write(key, &st, sizeof(int));
    }
    
    liblog(LOG_INFO, "DISKEXEC", "Disk Executive initialized successfully");
    return 0;
}

void exe_disk_main(void) {
    if (exe_disk_init() != 0) {
        liblog(LOG_ERROR, "DISKEXEC", "Initialization failed! Halting.");
        while (1) __asm__ volatile("hlt");
    }
    
    EXEC_SET_STATE(g_ecb, EXEC_STATE_RUNNING);
    liblog(LOG_INFO, "DISKEXEC", "Entering main loop...");
    
    exec_request_t req;
    exec_response_t resp;
    
    while (!EXEC_SHOULD_STOP(g_ecb)) {
        if (exe_request_queue_pop(g_req_queue, &req) == EXEC_OK) {
            exe_disk_dispatch(&req, &resp);
            exe_response_queue_push(g_resp_queue, &resp);
        } else {
            exe_yield();
        }
    }
    
    /* Cleanup */
    if (g_dev_handle >= 0) {
        exe_dev_close(DEV_DISK, g_dev_handle);
    }
    
    EXEC_SET_STATE(g_ecb, EXEC_STATE_STOPPED);
    liblog(LOG_WARN, "DISKEXEC", "Stopped. Halting.");
    while (1) __asm__ volatile("hlt");
}
