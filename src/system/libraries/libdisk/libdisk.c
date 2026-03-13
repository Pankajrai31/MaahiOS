/**
 * MaahiOS Disk Library (libdisk) - Implementation
 * 
 * Description:
 *   User-space library for block-level disk access.
 *   Auto-initializes on first call (discovers SHM, attaches).
 *   Returns error codes if Disk Executive is not yet running
 *   (never bypasses the executive layer with direct syscalls).
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#include "libdisk.h"
#include "../core/syscall_helpers.h"
#include "../libcell/libcell.h"
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
 * _libdisk_try_init - Try to connect to Disk Executive's SHM queues.
 * Called automatically on first use. Non-fatal if executive not ready.
 * Returns: 0 on success, -1 if executive not available yet.
 */
static int _libdisk_try_init(void) {
    if (g_initialized) return 0;
    
    /* Get our PID (only once) and seed msg_id to avoid collisions.
     * Multiple processes share the same executive response queue,
     * so msg_ids must be unique per process. */
    if (g_my_pid == 0) {
        g_my_pid = (uint32_t)syscall0(SYS_GETPID);
        g_msg_id = (g_my_pid << 16) | 1;
    }
    
    /* Read Disk Executive's request queue SHM ID from cell registry
     * via libcell → Cell Executive (proper layering) */
    int req_shm_id = -1;
    int result = libcell_read("system.exec.disk.req_shm",
                              &req_shm_id, sizeof(int));
    if (result < 0 || req_shm_id < 0) {
        return -1;  /* Executive not registered yet */
    }
    
    /* Read Disk Executive's response queue SHM ID */
    int resp_shm_id = -1;
    result = libcell_read("system.exec.disk.resp_shm",
                          &resp_shm_id, sizeof(int));
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
 * Send a request to Disk Executive and wait for response.
 * Returns EXEC_OK on success, negative error code on timeout/failure.
 */
static int _send_and_wait(exec_request_t *req, exec_response_t *resp) {
    if (!g_req_queue || !g_resp_queue) return EXEC_ERR_NOT_RUNNING;
    
    /* Assign unique ID */
    uint32_t my_id = g_msg_id++;
    req->msg_id     = my_id;
    req->sender_pid = g_my_pid;
    req->exec_id    = EXEC_ID_DISK;
    
    /* Push request */
    int push_result = exe_request_queue_push(g_req_queue, req);
    if (push_result != EXEC_OK) return push_result;
    
    /* Wait for matching response — sleep 1 tick between checks */
    for (int i = 0; i < 100; i++) {
        int pop_result = exe_response_queue_pop_by_id(g_resp_queue, my_id, resp);
        if (pop_result == EXEC_OK) {
            return EXEC_OK;
        }
        syscall1(SYS_SLEEP, 1);
    }
    
    return EXEC_ERR_TIMEOUT;
}

/*=============================================================================
 * PUBLIC API: INIT / SHUTDOWN
 *===========================================================================*/

int libdisk_init(void) {
    return _libdisk_try_init();
}

void libdisk_shutdown(void) {
    g_req_queue  = (void*)0;
    g_resp_queue = (void*)0;
    g_initialized = 0;
}

/*=============================================================================
 * PUBLIC API: DISK ENUMERATION
 *===========================================================================*/

int libdisk_list(disk_exec_info_t *disks, int max_disks) {
    if (!disks || max_disks <= 0) return EXEC_ERR_INVALID;
    if (!g_initialized) _libdisk_try_init();
    
    if (g_initialized) {
        /* Connected to Disk Executive — use SHM queue */
        exec_request_t req;
        exec_response_t resp;
        exe_memset(&req, 0, sizeof(req));
        
        req.func_id = DISK_OP_LIST_DISKS;
        req.payload_size = 0;
        
        int result = _send_and_wait(&req, &resp);
        if (result != EXEC_OK) return result;
        if (resp.status != EXEC_OK) return resp.status;
        
        /* Copy disk info from response */
        disk_list_resp_t *list = (disk_list_resp_t *)resp.payload;
        int count = (int)list->count;
        if (count > max_disks) count = max_disks;
        
        for (int i = 0; i < count; i++) {
            disks[i] = list->disks[i];
        }
        
        return (int)resp.result;  /* Total disk count */
    }
    
    /* Disk Executive not running — cannot enumerate disks */
    return 0;
}

int libdisk_get_count(void) {
    if (!g_initialized) _libdisk_try_init();
    
    if (g_initialized) {
        exec_request_t req;
        exec_response_t resp;
        exe_memset(&req, 0, sizeof(req));
        
        req.func_id = DISK_OP_LIST_DISKS;
        req.payload_size = 0;
        
        int result = _send_and_wait(&req, &resp);
        if (result != EXEC_OK) return result;
        if (resp.status != EXEC_OK) return resp.status;
        
        return (int)resp.result;  /* Total disk count */
    }
    
    /* Disk Executive not running — cannot count disks */
    return 0;
}

/*=============================================================================
 * PUBLIC API: DISK INFORMATION
 *===========================================================================*/

int libdisk_get_info(uint8_t disk_index, disk_exec_info_t *info) {
    if (!info) return EXEC_ERR_INVALID;
    if (!g_initialized) _libdisk_try_init();
    
    if (g_initialized) {
        exec_request_t req;
        exec_response_t resp;
        exe_memset(&req, 0, sizeof(req));
        
        req.func_id = DISK_OP_GET_INFO;
        disk_get_info_req_t *payload = (disk_get_info_req_t *)req.payload;
        payload->disk_index = disk_index;
        req.payload_size = sizeof(disk_get_info_req_t);
        
        int result = _send_and_wait(&req, &resp);
        if (result != EXEC_OK) return result;
        if (resp.status != EXEC_OK) return resp.status;
        
        disk_info_resp_t *info_resp = (disk_info_resp_t *)resp.payload;
        *info = info_resp->info;
        return EXEC_OK;
    }
    
    /* Fallback: can't get detailed info without executive */
    return EXEC_ERR_NOT_RUNNING;
}

int libdisk_get_status(uint8_t disk_index) {
    if (!g_initialized) _libdisk_try_init();
    
    if (g_initialized) {
        exec_request_t req;
        exec_response_t resp;
        exe_memset(&req, 0, sizeof(req));
        
        req.func_id = DISK_OP_GET_STATUS;
        disk_get_status_req_t *payload = (disk_get_status_req_t *)req.payload;
        payload->disk_index = disk_index;
        req.payload_size = sizeof(disk_get_status_req_t);
        
        int result = _send_and_wait(&req, &resp);
        if (result != EXEC_OK) return result;
        if (resp.status != EXEC_OK) return resp.status;
        
        return (int)resp.result;  /* DISK_STATUS_* */
    }
    
    /* Disk Executive not running — status unknown */
    return DISK_STATUS_OFFLINE;
}

int libdisk_get_sector_size(uint8_t disk_index) {
    if (!g_initialized) _libdisk_try_init();
    
    if (g_initialized) {
        exec_request_t req;
        exec_response_t resp;
        exe_memset(&req, 0, sizeof(req));
        
        req.func_id = DISK_OP_GET_SECTOR_SIZE;
        disk_get_sector_size_req_t *payload = (disk_get_sector_size_req_t *)req.payload;
        payload->disk_index = disk_index;
        req.payload_size = sizeof(disk_get_sector_size_req_t);
        
        int result = _send_and_wait(&req, &resp);
        if (result != EXEC_OK) return result;
        if (resp.status != EXEC_OK) return resp.status;
        
        return (int)resp.result;  /* Sector size in bytes */
    }
    
    /* Disk Executive not running — cannot query sector size */
    return EXEC_ERR_NOT_RUNNING;
}

/*=============================================================================
 * PUBLIC API: SECTOR OPERATIONS
 *===========================================================================*/

int libdisk_read_sector(uint8_t disk_index, uint32_t lba, uint32_t count) {
    if (!g_initialized) _libdisk_try_init();
    
    if (g_initialized) {
        exec_request_t req;
        exec_response_t resp;
        exe_memset(&req, 0, sizeof(req));
        
        req.func_id = DISK_OP_READ_SECTOR;
        disk_read_sector_req_t *payload = (disk_read_sector_req_t *)req.payload;
        payload->disk_index = disk_index;
        payload->lba = lba;
        payload->count = count;
        req.payload_size = sizeof(disk_read_sector_req_t);
        
        int result = _send_and_wait(&req, &resp);
        if (result != EXEC_OK) return result;
        if (resp.status != EXEC_OK) return resp.status;
        
        return (int)resp.result;  /* SHM ID containing sector data */
    }
    
    /* Fallback: direct device read — limited, just returns first disk's info */
    return EXEC_ERR_NOT_RUNNING;
}

/*=============================================================================
 * PUBLIC API: DISK FORMAT
 *===========================================================================*/

int libdisk_format(uint8_t disk_index, const char *label) {
    if (!g_initialized) _libdisk_try_init();
    if (!g_initialized) return EXEC_ERR_NOT_RUNNING;
    
    exec_request_t req;
    exec_response_t resp;
    exe_memset(&req, 0, sizeof(req));
    
    req.func_id = DISK_OP_FORMAT;
    disk_format_req_t *payload = (disk_format_req_t *)req.payload;
    payload->disk_index = disk_index;
    
    /* Copy label or default */
    if (label) {
        int i;
        for (i = 0; i < 31 && label[i]; i++) {
            payload->label[i] = label[i];
        }
        payload->label[i] = '\0';
    } else {
        const char *def = "MaahiOS";
        int i;
        for (i = 0; def[i]; i++) {
            payload->label[i] = def[i];
        }
        payload->label[i] = '\0';
    }
    
    req.payload_size = sizeof(disk_format_req_t);
    
    int result = _send_and_wait(&req, &resp);
    if (result != EXEC_OK) return result;
    if (resp.status != EXEC_OK) return resp.status;
    
    return (int)resp.result;  /* 0 on success, negative on error */
}
