/**
 * MaahiOS Filesystem Library (libfs) - Implementation
 * 
 * Description:
 *   User-space library for file and directory access.
 *   Auto-initializes on first call (discovers SHM, attaches).
 *   Communicates with FS Executive via SHM request/response queues.
 *   Returns file data via SHM blocks allocated by the FS Executive.
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#include "libfs.h"
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
 * _libfs_try_init - Try to connect to FS Executive's SHM queues.
 * Called automatically on first use. Non-fatal if executive not ready.
 * Returns: 0 on success, -1 if executive not available yet.
 */
static int _libfs_try_init(void) {
    if (g_initialized) return 0;

    /* Get our PID (only once) and seed msg_id to avoid collisions */
    if (g_my_pid == 0) {
        g_my_pid = (uint32_t)syscall0(SYS_GETPID);
        g_msg_id = (g_my_pid << 16) | 1;
    }

    /* Read FS Executive's request queue SHM ID from cell registry */
    int req_shm_id = -1;
    int result = syscall3(SYS_CELL_READ,
                          (uint32_t)"system.exec.fs.req_shm",
                          (uint32_t)&req_shm_id, sizeof(int));
    if (result < 0 || req_shm_id < 0) {
        return -1;  /* Executive not registered yet */
    }

    /* Read FS Executive's response queue SHM ID */
    int resp_shm_id = -1;
    result = syscall3(SYS_CELL_READ,
                      (uint32_t)"system.exec.fs.resp_shm",
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
 * Send a request to FS Executive and wait for response.
 * Returns EXEC_OK on success, negative error code on timeout/failure.
 */
static int _send_and_wait(exec_request_t *req, exec_response_t *resp) {
    if (!g_req_queue || !g_resp_queue) return EXEC_ERR_NOT_RUNNING;

    /* Assign unique ID */
    uint32_t my_id = g_msg_id++;
    req->msg_id     = my_id;
    req->sender_pid = g_my_pid;
    req->exec_id    = EXEC_ID_FS;

    /* Push request */
    int push_result = exe_request_queue_push(g_req_queue, req);
    if (push_result != EXEC_OK) return push_result;

    /* Wait for matching response — sleep 1 tick between checks */
    for (int i = 0; i < 200; i++) {
        int pop_result = exe_response_queue_pop_by_id(g_resp_queue, my_id, resp);
        if (pop_result == EXEC_OK) {
            return EXEC_OK;
        }
        syscall1(SYS_SLEEP, 1);
    }

    return EXEC_ERR_TIMEOUT;
}

/*=============================================================================
 * INTERNAL: STRING HELPERS (no libc in freestanding)
 *===========================================================================*/

static void _fs_str_copy(char *dst, const char *src, int max) {
    int i = 0;
    if (src) {
        while (i < max - 1 && src[i]) {
            dst[i] = src[i];
            i++;
        }
    }
    dst[i] = '\0';
}

static void _fs_memset(void *dst, uint8_t val, uint32_t size) {
    uint8_t *d = (uint8_t *)dst;
    for (uint32_t i = 0; i < size; i++) {
        d[i] = val;
    }
}

static void _fs_memcpy(void *dst, const void *src, uint32_t size) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (uint32_t i = 0; i < size; i++) {
        d[i] = s[i];
    }
}

/*=============================================================================
 * PUBLIC API: INIT / SHUTDOWN
 *===========================================================================*/

int libfs_init(void) {
    return _libfs_try_init();
}

void libfs_shutdown(void) {
    g_req_queue   = (void*)0;
    g_resp_queue  = (void*)0;
    g_initialized = 0;
}

/*=============================================================================
 * PUBLIC API: DIRECTORY OPERATIONS
 *===========================================================================*/

int libfs_list_dir(const char *path, fs_file_entry_t *entries, int max_entries) {
    if (!path || !entries || max_entries <= 0) return EXEC_ERR_INVALID;
    if (!g_initialized) _libfs_try_init();
    if (!g_initialized) return EXEC_ERR_NOT_RUNNING;

    exec_request_t req;
    exec_response_t resp;
    _fs_memset(&req, 0, sizeof(req));

    req.func_id = FS_OP_LIST_DIR;
    fs_list_dir_req_t *payload = (fs_list_dir_req_t *)req.payload;
    _fs_str_copy(payload->path, path, 128);
    req.payload_size = sizeof(fs_list_dir_req_t);

    int result = _send_and_wait(&req, &resp);
    if (result != EXEC_OK) return result;
    if (resp.status != EXEC_OK) return resp.status;

    int count = (int)resp.result;
    if (count == 0) return 0;

    /* Get SHM ID from response */
    fs_list_dir_resp_t *resp_payload = (fs_list_dir_resp_t *)resp.payload;
    int shm_id = resp_payload->shm_id;

    /* Attach to SHM block containing entries */
    fs_file_entry_t *shm_data = (fs_file_entry_t *)syscall2(SYS_SHM_ATTACH, shm_id, 0);
    if (!shm_data || (uint32_t)shm_data == 0xFFFFFFFF) {
        return EXEC_ERR_NO_MEMORY;
    }

    /* Copy entries to caller's buffer */
    int to_copy = (count > max_entries) ? max_entries : count;
    _fs_memcpy(entries, shm_data, (uint32_t)to_copy * sizeof(fs_file_entry_t));

    /* Cleanup SHM */
    syscall1(SYS_SHM_DETACH, shm_id);
    syscall1(SYS_SHM_DESTROY, shm_id);

    return count;
}

int libfs_file_count(const char *path) {
    if (!path) return EXEC_ERR_INVALID;
    if (!g_initialized) _libfs_try_init();
    if (!g_initialized) return EXEC_ERR_NOT_RUNNING;

    exec_request_t req;
    exec_response_t resp;
    _fs_memset(&req, 0, sizeof(req));

    req.func_id = FS_OP_FILE_COUNT;
    fs_file_count_req_t *payload = (fs_file_count_req_t *)req.payload;
    _fs_str_copy(payload->path, path, 128);
    req.payload_size = sizeof(fs_file_count_req_t);

    int result = _send_and_wait(&req, &resp);
    if (result != EXEC_OK) return result;
    if (resp.status != EXEC_OK) return resp.status;

    return (int)resp.result;
}

/*=============================================================================
 * PUBLIC API: FILE OPERATIONS
 *===========================================================================*/

int libfs_read_file(const char *dir_path, const char *filename,
                    void *buffer, uint32_t max_size) {
    if (!dir_path || !filename || !buffer || max_size == 0)
        return EXEC_ERR_INVALID;
    if (!g_initialized) _libfs_try_init();
    if (!g_initialized) return EXEC_ERR_NOT_RUNNING;

    exec_request_t req;
    exec_response_t resp;
    _fs_memset(&req, 0, sizeof(req));

    req.func_id = FS_OP_READ_FILE;
    fs_read_file_req_t *payload = (fs_read_file_req_t *)req.payload;
    _fs_str_copy(payload->dir_path, dir_path, 64);
    _fs_str_copy(payload->filename, filename, 64);
    payload->max_size = max_size;
    req.payload_size = sizeof(fs_read_file_req_t);

    int result = _send_and_wait(&req, &resp);
    if (result != EXEC_OK) return result;
    if (resp.status != EXEC_OK) return resp.status;

    int bytes_read = (int)resp.result;
    if (bytes_read == 0) return 0;

    /* Get SHM ID from response */
    fs_read_file_resp_t *resp_payload = (fs_read_file_resp_t *)resp.payload;
    int shm_id = resp_payload->shm_id;

    /* Attach to SHM block containing file data */
    void *shm_data = (void *)syscall2(SYS_SHM_ATTACH, shm_id, 0);
    if (!shm_data || (uint32_t)shm_data == 0xFFFFFFFF) {
        return EXEC_ERR_NO_MEMORY;
    }

    /* Copy to caller's buffer */
    uint32_t to_copy = ((uint32_t)bytes_read > max_size) ? max_size : (uint32_t)bytes_read;
    _fs_memcpy(buffer, shm_data, to_copy);

    /* Cleanup SHM */
    syscall1(SYS_SHM_DETACH, shm_id);
    syscall1(SYS_SHM_DESTROY, shm_id);

    return (int)to_copy;
}
