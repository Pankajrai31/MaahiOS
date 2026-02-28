/**
 * MaahiOS Filesystem Executive Implementation
 * 
 * Description:
 *   File-level access executive. Lists directories, reads files.
 *   Abstracts filesystem type (ISO9660 now, MFS stubs for future).
 *   Talks to kernel ISO9660 driver via SYS_FS_* syscalls.
 * 
 *   Loaded by sysman as GRUB module 6.
 *   Uses liblog for logging (auto-init).
 *   Uses libcell for cell registration (auto-init).
 *   Dual SHM queues (request + response).
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#include "fs_executive.h"
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
 * KERNEL FILE ENTRY TYPE (matches iso9660.h iso_file_entry_t exactly)
 * Duplicated here since Ring 3 can't include kernel headers.
 *===========================================================================*/

typedef struct {
    char     name[64];
    uint32_t size;
    uint32_t lba;
    uint8_t  is_directory;
} kernel_file_entry_t;

/* Buffer for kernel syscall results */
#define FS_MAX_ENTRIES      32
static kernel_file_entry_t g_entries_buf[FS_MAX_ENTRIES];

/*=============================================================================
 * EXECUTIVE STATE
 *===========================================================================*/

static exec_control_block_t  *g_ecb = NULL;
static exec_request_queue_t  *g_req_queue = NULL;
static exec_response_queue_t *g_resp_queue = NULL;
static int g_req_queue_shm_id  = -1;
static int g_resp_queue_shm_id = -1;

/*=============================================================================
 * MFS STUBS (MaahiOS File System — future implementation)
 *
 * These functions are placeholders for the future MFS filesystem.
 * They currently return errors. When MFS is implemented, they will
 * be filled in with actual MFS driver calls.
 *===========================================================================*/

/*
static int mfs_list_dir(const char *path, kernel_file_entry_t *entries, int max) {
    (void)path; (void)entries; (void)max;
    return -1;  // MFS not implemented yet
}

static int mfs_read_file(const char *dir_path, const char *filename,
                         void *buffer, uint32_t max_size) {
    (void)dir_path; (void)filename; (void)buffer; (void)max_size;
    return -1;  // MFS not implemented yet
}

static int mfs_file_count(const char *path) {
    (void)path;
    return -1;  // MFS not implemented yet
}
*/

/*=============================================================================
 * ISO9660 OPERATIONS (via kernel SYS_FS_* syscalls)
 *===========================================================================*/

/**
 * List directory via kernel syscall.
 * Returns entry count on success, negative on error.
 */
static int iso_list_dir(const char *path, kernel_file_entry_t *entries, int max) {
    return syscall3(SYS_FS_LIST_DIR, (uint32_t)path, (uint32_t)entries, (uint32_t)max);
}

/**
 * Read file via kernel syscall.
 * Returns bytes read on success, negative on error.
 */
static int iso_read_file(const char *dir_path, const char *filename,
                         void *buffer, uint32_t max_size) {
    return (int)syscall4(SYS_FS_READ_FILE,
                         (uint32_t)dir_path, (uint32_t)filename,
                         (uint32_t)buffer, (uint32_t)max_size);
}

/**
 * Get file count via kernel syscall.
 */
static int iso_file_count(const char *path) {
    return syscall1(SYS_FS_FILE_COUNT, (uint32_t)path);
}

/*=============================================================================
 * REQUEST HANDLERS
 *===========================================================================*/

static void exe_fs_handle_ping(const exec_request_t *req, exec_response_t *resp) {
    resp->msg_id = req->msg_id;
    resp->status = EXEC_OK;
    resp->result = 1;
    resp->payload_size = 0;
}

/**
 * Handle LIST_DIR: list directory contents.
 * Creates a SHM block with fs_file_entry_t array.
 * Response result = count, payload = SHM ID.
 */
static void exe_fs_handle_list_dir(const exec_request_t *req, exec_response_t *resp) {
    fs_list_dir_req_t *payload = (fs_list_dir_req_t *)req->payload;
    const char *path = payload->path;

    liblog(LOG_DEBUG, "FSEXEC", "LIST_DIR request");

    /* Call kernel to list directory */
    int count = iso_list_dir(path, g_entries_buf, FS_MAX_ENTRIES);

    if (count <= 0) {
        liblog(LOG_WARN, "FSEXEC", "LIST_DIR: no entries or error");
        resp->msg_id = req->msg_id;
        resp->status = (count == 0) ? EXEC_OK : EXEC_ERR_NOT_FOUND;
        resp->result = 0;
        resp->payload_size = 0;
        return;
    }

    liblog_hex(LOG_DEBUG, "FSEXEC", "LIST_DIR: entries found:", (uint32_t)count);

    /* Create SHM block for results */
    uint32_t shm_size = (uint32_t)count * sizeof(fs_file_entry_t);
    int shm_id = exe_shm_create(shm_size);
    if (shm_id < 0) {
        liblog(LOG_ERROR, "FSEXEC", "LIST_DIR: failed to create SHM");
        resp->msg_id = req->msg_id;
        resp->status = EXEC_ERR_NO_MEMORY;
        resp->payload_size = 0;
        return;
    }

    fs_file_entry_t *shm_data = (fs_file_entry_t *)exe_shm_attach(shm_id);
    if (!shm_data) {
        liblog(LOG_ERROR, "FSEXEC", "LIST_DIR: failed to attach SHM");
        resp->msg_id = req->msg_id;
        resp->status = EXEC_ERR_NO_MEMORY;
        resp->payload_size = 0;
        return;
    }

    /* Convert kernel entries to user entries */
    for (int i = 0; i < count; i++) {
        exe_memset(&shm_data[i], 0, sizeof(fs_file_entry_t));
        exe_str_copy(shm_data[i].name, g_entries_buf[i].name, 44);
        shm_data[i].size = g_entries_buf[i].size;
        shm_data[i].is_directory = g_entries_buf[i].is_directory;
        shm_data[i].fs_type = FS_TYPE_ISO9660;
    }

    /* Build response */
    fs_list_dir_resp_t *resp_payload = (fs_list_dir_resp_t *)resp->payload;
    resp_payload->shm_id = shm_id;

    resp->msg_id = req->msg_id;
    resp->status = EXEC_OK;
    resp->result = (uint32_t)count;
    resp->payload_size = sizeof(fs_list_dir_resp_t);
}

/**
 * Handle READ_FILE: read file contents into SHM block.
 * Response result = bytes read, payload = SHM ID.
 */
static void exe_fs_handle_read_file(const exec_request_t *req, exec_response_t *resp) {
    fs_read_file_req_t *payload = (fs_read_file_req_t *)req->payload;
    const char *dir_path = payload->dir_path;
    const char *filename = payload->filename;
    uint32_t max_size = payload->max_size;

    liblog(LOG_DEBUG, "FSEXEC", "READ_FILE request");

    if (max_size == 0 || max_size > 0x100000) {
        /* Limit to 1MB per read */
        max_size = 0x100000;
    }

    /* Create SHM block for file data */
    int shm_id = exe_shm_create(max_size);
    if (shm_id < 0) {
        liblog(LOG_ERROR, "FSEXEC", "READ_FILE: failed to create SHM");
        resp->msg_id = req->msg_id;
        resp->status = EXEC_ERR_NO_MEMORY;
        resp->payload_size = 0;
        return;
    }

    void *shm_buf = exe_shm_attach(shm_id);
    if (!shm_buf) {
        liblog(LOG_ERROR, "FSEXEC", "READ_FILE: failed to attach SHM");
        resp->msg_id = req->msg_id;
        resp->status = EXEC_ERR_NO_MEMORY;
        resp->payload_size = 0;
        return;
    }

    /* Read file via kernel syscall */
    int bytes_read = iso_read_file(dir_path, filename, shm_buf, max_size);

    if (bytes_read < 0) {
        liblog(LOG_WARN, "FSEXEC", "READ_FILE: file not found or read error");
        /* Destroy unused SHM */
        syscall1(SYS_SHM_DESTROY, shm_id);
        resp->msg_id = req->msg_id;
        resp->status = EXEC_ERR_NOT_FOUND;
        resp->payload_size = 0;
        return;
    }

    liblog_hex(LOG_DEBUG, "FSEXEC", "READ_FILE: bytes read:", (uint32_t)bytes_read);

    /* Build response */
    fs_read_file_resp_t *resp_payload = (fs_read_file_resp_t *)resp->payload;
    resp_payload->shm_id = shm_id;

    resp->msg_id = req->msg_id;
    resp->status = EXEC_OK;
    resp->result = (uint32_t)bytes_read;
    resp->payload_size = sizeof(fs_read_file_resp_t);
}

/**
 * Handle FILE_COUNT: get number of entries in a directory.
 * Response result = count.
 */
static void exe_fs_handle_file_count(const exec_request_t *req, exec_response_t *resp) {
    fs_file_count_req_t *payload = (fs_file_count_req_t *)req->payload;
    const char *path = payload->path;

    int count = iso_file_count(path);

    resp->msg_id = req->msg_id;
    resp->status = (count >= 0) ? EXEC_OK : EXEC_ERR_NOT_FOUND;
    resp->result = (count >= 0) ? (uint32_t)count : 0;
    resp->payload_size = 0;
}

/*=============================================================================
 * REQUEST DISPATCHER
 *===========================================================================*/

static void exe_fs_dispatch(const exec_request_t *req, exec_response_t *resp) {
    exe_memset(resp, 0, sizeof(exec_response_t));

    switch (req->func_id) {
        case EXEC_OP_PING:
            exe_fs_handle_ping(req, resp);
            break;

        case EXEC_OP_SHUTDOWN:
            g_ecb->stop_requested = 1;
            resp->msg_id = req->msg_id;
            resp->status = EXEC_OK;
            break;

        case FS_OP_LIST_DIR:
            exe_fs_handle_list_dir(req, resp);
            break;

        case FS_OP_READ_FILE:
            exe_fs_handle_read_file(req, resp);
            break;

        case FS_OP_FILE_COUNT:
            exe_fs_handle_file_count(req, resp);
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

static int exe_fs_init(void) {
    liblog(LOG_INFO, "FSEXEC", "Filesystem Executive initializing...");

    /* Create SHM queues */
    g_req_queue_shm_id = exe_shm_create(sizeof(exec_request_queue_t));
    if (g_req_queue_shm_id < 0) {
        liblog(LOG_ERROR, "FSEXEC", "Failed to create request queue SHM");
        return -1;
    }

    g_req_queue = (exec_request_queue_t *)exe_shm_attach(g_req_queue_shm_id);
    if (!g_req_queue) {
        liblog(LOG_ERROR, "FSEXEC", "Failed to attach request queue SHM");
        return -1;
    }
    exe_request_queue_init(g_req_queue);
    liblog_hex(LOG_INFO, "FSEXEC", "Request queue SHM ID:", g_req_queue_shm_id);

    g_resp_queue_shm_id = exe_shm_create(sizeof(exec_response_queue_t));
    if (g_resp_queue_shm_id < 0) {
        liblog(LOG_ERROR, "FSEXEC", "Failed to create response queue SHM");
        return -1;
    }

    g_resp_queue = (exec_response_queue_t *)exe_shm_attach(g_resp_queue_shm_id);
    if (!g_resp_queue) {
        liblog(LOG_ERROR, "FSEXEC", "Failed to attach response queue SHM");
        return -1;
    }
    exe_response_queue_init(g_resp_queue);
    liblog_hex(LOG_INFO, "FSEXEC", "Response queue SHM ID:", g_resp_queue_shm_id);

    /* Set up control block */
    static exec_control_block_t local_ecb;
    g_ecb = &local_ecb;
    exe_str_copy(g_ecb->name, "fs_executive", EXEC_NAME_MAX);
    g_ecb->exec_id = EXEC_ID_FS;
    g_ecb->state = EXEC_STATE_STARTING;
    g_ecb->priority = PRIORITY_HIGH;
    g_ecb->request_queue_shm_id = g_req_queue_shm_id;
    g_ecb->response_queue_shm_id = g_resp_queue_shm_id;

    /* Write queue SHM IDs to cells for discovery */
    libcell_write("system.exec.fs.req_shm", &g_req_queue_shm_id, sizeof(int));
    libcell_write("system.exec.fs.resp_shm", &g_resp_queue_shm_id, sizeof(int));

    /* Publish filesystem type info */
    int fs_type = FS_TYPE_ISO9660;
    libcell_write("system.fs.type", &fs_type, sizeof(int));

    liblog(LOG_INFO, "FSEXEC", "Filesystem Executive initialized (ISO9660)");
    return 0;
}

void exe_fs_main(void) {
    if (exe_fs_init() != 0) {
        liblog(LOG_ERROR, "FSEXEC", "Initialization failed! Halting.");
        while (1) __asm__ volatile("hlt");
    }

    EXEC_SET_STATE(g_ecb, EXEC_STATE_RUNNING);
    liblog(LOG_INFO, "FSEXEC", "Entering main loop...");

    exec_request_t req;
    exec_response_t resp;

    while (!EXEC_SHOULD_STOP(g_ecb)) {
        if (exe_request_queue_pop(g_req_queue, &req) == EXEC_OK) {
            exe_fs_dispatch(&req, &resp);
            exe_response_queue_push(g_resp_queue, &resp);
        } else {
            exe_yield();
        }
    }

    EXEC_SET_STATE(g_ecb, EXEC_STATE_STOPPED);
    liblog(LOG_WARN, "FSEXEC", "Stopped. Halting.");
    while (1) __asm__ volatile("hlt");
}
