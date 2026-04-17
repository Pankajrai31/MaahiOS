/**
 * MaahiOS Process Library (libprocess) - Implementation
 * 
 * Description:
 *   User-space library for process management.
 *   Auto-initializes on first call (discovers SHM, attaches).
 *   Falls back to direct SYS_PROCESS_* kernel syscalls if
 *   Process Executive is not yet running.
 * 
 * Usage:
 *   int pid = libprocess_create(4, 0, 16384);  // just call it
 *   No init() needed — handled automatically.
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#include "libprocess.h"
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
 * _libprocess_try_init - Try to connect to Process Executive's SHM queues.
 * Called automatically on first use. Non-fatal if executive not ready.
 * Returns: 0 on success, -1 if executive not available yet.
 */
static int _libprocess_try_init(void) {
    if (g_initialized) return 0;
    
    /* Get our PID (only once) and seed msg_id to avoid collisions.
     * Multiple processes share the same executive response queue,
     * so msg_ids must be unique per process. */
    if (g_my_pid == 0) {
        g_my_pid = (uint32_t)syscall0(SYS_GETPID);
        g_msg_id = (g_my_pid << 16) | 1;
    }
    
    /* Read Process Executive's request queue SHM ID from cell registry */
    int req_shm_id = -1;
    int result = syscall3(SYS_CELL_READ,
                          (uint32_t)"system.exec.process.req_shm",
                          (uint32_t)&req_shm_id, sizeof(int));
    if (result < 0 || req_shm_id < 0) {
        return -1;  /* Executive not registered yet */
    }
    
    /* Read Process Executive's response queue SHM ID */
    int resp_shm_id = -1;
    result = syscall3(SYS_CELL_READ,
                      (uint32_t)"system.exec.process.resp_shm",
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
 * Send a request to Process Executive and wait for response.
 * Returns EXEC_OK on success, negative error code on timeout/failure.
 */
static int _send_and_wait(exec_request_t *req, exec_response_t *resp) {
    if (!g_req_queue || !g_resp_queue) return EXEC_ERR_NOT_RUNNING;
    
    /* Assign unique ID */
    uint32_t my_id = g_msg_id++;
    req->msg_id     = my_id;
    req->sender_pid = g_my_pid;
    req->exec_id    = EXEC_ID_PROCESS;
    
    /* Push request */
    int push_result = exe_request_queue_push(g_req_queue, req);
    if (push_result != EXEC_OK) return push_result;
    
    /* Wait for matching response — sleep 1 tick between checks
     * so PIT can switch to Process Executive to process our request */
    for (int i = 0; i < 100; i++) {
        int pop_result = exe_response_queue_pop_by_id(g_resp_queue, my_id, resp);
        if (pop_result == EXEC_OK) {
            return EXEC_OK;
        }
        exe_poll_heartbeat();
        /* Sleep 1 PIT tick (~20ms at 50Hz) to yield CPU */
        syscall1(SYS_SLEEP, 1);
    }
    
    return EXEC_ERR_TIMEOUT;
}

/*=============================================================================
 * PUBLIC API: INIT / SHUTDOWN (optional — auto-init handles it)
 *===========================================================================*/

int libprocess_init(void) {
    return _libprocess_try_init();
}

void libprocess_shutdown(void) {
    g_req_queue  = (void*)0;
    g_resp_queue = (void*)0;
    g_initialized = 0;
}

/*=============================================================================
 * PUBLIC API: PROCESS OPERATIONS
 * Each function: auto-inits → if connected, use executive → else fallback
 *===========================================================================*/

/* Standard virtual base for all user processes (per-process page directories) */
#define PROCESS_VIRTUAL_BASE    0x10000000

int libprocess_create(uint32_t module_index, uint32_t load_address, uint32_t bss_size) {
    /* Auto-init on first call */
    if (!g_initialized) _libprocess_try_init();
    
    if (g_initialized) {
        /* Connected to Process Executive — use SHM queue.
         * Process Executive now handles per-process page directories
         * internally (ignores load_address, uses PROCESS_VIRTUAL_BASE). */
        exec_request_t req;
        exec_response_t resp;
        exe_memset(&req, 0, sizeof(req));
        
        req.func_id = PROC_OP_CREATE;
        proc_create_req_t *payload = (proc_create_req_t *)req.payload;
        payload->module_index = module_index;
        payload->load_address = load_address;
        payload->bss_size = bss_size;
        req.payload_size = sizeof(proc_create_req_t);
        
        int result = _send_and_wait(&req, &resp);
        if (result != EXEC_OK) return result;
        return resp.status == EXEC_OK ? (int)resp.result : resp.status;
    }
    
    /* Fallback: direct kernel syscalls (Process Executive not running yet).
     * Read module address and size from GRUB module manager, then use
     * SYS_PROCESS_EXEC to create with per-process page directory. */
    uint32_t mod_addr = (uint32_t)syscall1(SYS_MOD_GET_ADDR, (int)module_index);
    if (mod_addr == 0) return -1;
    
    uint32_t mod_size = (uint32_t)syscall1(SYS_MOD_GET_SIZE, (int)module_index);
    if (mod_size == 0) return -1;
    
    return syscall5(SYS_PROCESS_EXEC, PROCESS_VIRTUAL_BASE,
                    (int)mod_addr, (int)mod_size, 0, (int)bss_size);
}

int libprocess_exec(uint32_t base_address, const void *binary_data,
                    uint32_t binary_size, uint32_t entry_offset,
                    uint32_t bss_size) {
    /* Auto-init on first call */
    if (!g_initialized) _libprocess_try_init();
    
    if (g_initialized) {
        /*
         * SHM-based binary transfer to Process Executive.
         *
         * With per-process page directories, binary_data is a virtual address
         * valid only in our address space. We cannot pass the pointer to
         * another process. Instead:
         *   1. Create a SHM region large enough for the binary
         *   2. Copy binary data into the SHM (shared physical pages)
         *   3. Send the SHM ID (an integer handle) to the executive
         *   4. Executive attaches the same SHM, reads the binary, creates process
         *   5. We destroy the SHM after the response
         *
         * This follows the same pattern used by libfs (data via SHM blocks).
         */
        
        /* Step 1: Create SHM for binary transfer */
        int bin_shm_id = syscall1(SYS_SHM_CREATE, (int)binary_size);
        if (bin_shm_id < 0) {
            return EXEC_ERR_NO_MEMORY;
        }
        
        /* Step 2: Attach and copy binary into SHM */
        void *shm_ptr = (void *)syscall2(SYS_SHM_ATTACH, bin_shm_id, 0);
        if (!shm_ptr || (uint32_t)shm_ptr == 0xFFFFFFFF) {
            syscall1(SYS_SHM_DESTROY, bin_shm_id);
            return EXEC_ERR_NO_MEMORY;
        }
        
        /* Copy binary data into SHM (word-aligned fast copy) */
        const uint8_t *src = (const uint8_t *)binary_data;
        uint8_t *dst = (uint8_t *)shm_ptr;
        uint32_t i = 0;
        /* Word-copy aligned portion */
        uint32_t words = binary_size / 4;
        const uint32_t *src32 = (const uint32_t *)src;
        uint32_t *dst32 = (uint32_t *)dst;
        for (uint32_t w = 0; w < words; w++) {
            dst32[w] = src32[w];
        }
        /* Byte-copy remainder */
        for (i = words * 4; i < binary_size; i++) {
            dst[i] = src[i];
        }
        
        /* Step 3: Send request with SHM ID (not pointer) */
        exec_request_t req;
        exec_response_t resp;
        exe_memset(&req, 0, sizeof(req));
        
        req.func_id = PROC_OP_EXEC;
        proc_exec_req_t *payload = (proc_exec_req_t *)req.payload;
        payload->base_address  = base_address;
        payload->binary_shm_id = bin_shm_id;
        payload->binary_size   = binary_size;
        payload->entry_offset  = entry_offset;
        payload->bss_size      = bss_size;
        req.payload_size = sizeof(proc_exec_req_t);
        
        int result = _send_and_wait(&req, &resp);
        
        /* Step 4: Cleanup — detach and destroy SHM regardless of result */
        syscall1(SYS_SHM_DETACH, bin_shm_id);
        syscall1(SYS_SHM_DESTROY, bin_shm_id);
        
        if (result != EXEC_OK) return result;
        return resp.status == EXEC_OK ? (int)resp.result : resp.status;
    }
    
    /* Fallback: direct kernel syscall (Process Executive not running) */
    return syscall5(SYS_PROCESS_EXEC, (int)base_address,
                    (int)binary_data, (int)binary_size, (int)entry_offset,
                    (int)bss_size);
}

int libprocess_kill(int32_t pid) {
    if (!g_initialized) _libprocess_try_init();
    
    if (g_initialized) {
        exec_request_t req;
        exec_response_t resp;
        exe_memset(&req, 0, sizeof(req));
        
        req.func_id = PROC_OP_KILL;
        proc_kill_req_t *payload = (proc_kill_req_t *)req.payload;
        payload->pid = pid;
        req.payload_size = sizeof(proc_kill_req_t);
        
        int result = _send_and_wait(&req, &resp);
        if (result != EXEC_OK) return result;
        return resp.status;
    }
    
    /* Fallback: direct kernel syscall */
    return syscall1(SYS_PROCESS_KILL, pid);
}

int libprocess_get_info(int32_t pid, process_info_t *info) {
    if (!info) return EXEC_ERR_INVALID;
    if (!g_initialized) _libprocess_try_init();
    
    if (g_initialized) {
        exec_request_t req;
        exec_response_t resp;
        exe_memset(&req, 0, sizeof(req));
        
        req.func_id = PROC_OP_GET_INFO;
        proc_info_req_t *payload = (proc_info_req_t *)req.payload;
        payload->pid = pid;
        req.payload_size = sizeof(proc_info_req_t);
        
        int result = _send_and_wait(&req, &resp);
        if (result != EXEC_OK) return result;
        if (resp.status != EXEC_OK) return resp.status;
        
        proc_info_resp_t *resp_data = (proc_info_resp_t *)resp.payload;
        *info = resp_data->info;
        return EXEC_OK;
    }
    
    /* Fallback: direct kernel syscall — returns 48-byte info */
    process_info_t raw;
    exe_memset(&raw, 0, sizeof(raw));
    int result = syscall2(SYS_PROCESS_INFO, pid, (int)&raw);
    if (result >= 0) {
        *info = raw;
    }
    return result;
}

int libprocess_get_count(void) {
    if (!g_initialized) _libprocess_try_init();
    
    if (g_initialized) {
        exec_request_t req;
        exec_response_t resp;
        exe_memset(&req, 0, sizeof(req));
        
        req.func_id = PROC_OP_GET_COUNT;
        req.payload_size = 0;
        
        int result = _send_and_wait(&req, &resp);
        if (result != EXEC_OK) return result;
        return resp.status == EXEC_OK ? (int)resp.result : resp.status;
    }
    
    /* Fallback: direct kernel syscall */
    return syscall0(SYS_PROCESS_GET_COUNT);
}

int libprocess_list(process_info_t *infos, int max) {
    if (!infos || max <= 0) return EXEC_ERR_INVALID;
    if (!g_initialized) _libprocess_try_init();
    
    if (g_initialized) {
        exec_request_t req;
        exec_response_t resp;
        exe_memset(&req, 0, sizeof(req));
        
        req.func_id = PROC_OP_LIST;
        req.payload_size = 0;
        
        int result = _send_and_wait(&req, &resp);
        if (result != EXEC_OK) return result;
        if (resp.status != EXEC_OK) return resp.status;
        
        int count = (int)resp.result;
        if (count <= 0) return 0;
        
        /* Get SHM ID from response payload */
        proc_list_resp_t *pl = (proc_list_resp_t *)resp.payload;
        if (pl->shm_id < 0) return EXEC_ERR_INVALID;
        
        /* Attach SHM, copy process entries (48 bytes each), cleanup */
        void *shm_ptr = (void *)syscall2(SYS_SHM_ATTACH, pl->shm_id, 0);
        if (!shm_ptr || (uint32_t)shm_ptr == 0xFFFFFFFF) {
            syscall1(SYS_SHM_DESTROY, pl->shm_id);
            return EXEC_ERR_NO_MEMORY;
        }
        
        int to_copy = (count < max) ? count : max;
        uint8_t *src = (uint8_t *)shm_ptr;
        for (int i = 0; i < to_copy; i++) {
            uint8_t *entry = src + (i * 48);
            infos[i].pid          = *(int32_t *)(entry + 0);
            infos[i].state        = *(uint32_t *)(entry + 4);
            for (int j = 0; j < PROC_NAME_MAX; j++)
                infos[i].name[j] = (char)entry[8 + j];
            infos[i].type         = entry[40];
            infos[i].memory_alloc = *(uint32_t *)(entry + 44);
        }
        
        syscall1(SYS_SHM_DETACH, pl->shm_id);
        syscall1(SYS_SHM_DESTROY, pl->shm_id);
        
        return to_copy;
    }
    
    /* Fallback: direct kernel syscall — 48 bytes per entry */
    uint8_t buf[48 * 32];  /* Max 32 entries */
    int fmax = (max < 32) ? max : 32;
    int count = syscall2(SYS_PROCESS_LIST, (int)buf, fmax);
    if (count < 0) return count;
    
    for (int i = 0; i < count; i++) {
        uint8_t *entry = buf + (i * 48);
        infos[i].pid          = *(int32_t *)(entry + 0);
        infos[i].state        = *(uint32_t *)(entry + 4);
        for (int j = 0; j < PROC_NAME_MAX; j++)
            infos[i].name[j] = (char)entry[8 + j];
        infos[i].type         = entry[40];
        infos[i].memory_alloc = *(uint32_t *)(entry + 44);
    }
    return count;
}

int libprocess_set_name(int32_t pid, const char *name, uint8_t type) {
    if (!name || pid <= 0) return EXEC_ERR_INVALID;
    if (!g_initialized) _libprocess_try_init();
    
    if (g_initialized) {
        exec_request_t req;
        exec_response_t resp;
        exe_memset(&req, 0, sizeof(req));
        
        req.func_id = PROC_OP_SET_NAME;
        proc_set_name_req_t *payload = (proc_set_name_req_t *)req.payload;
        payload->pid = pid;
        payload->type = type;
        /* Safe copy name */
        int i;
        for (i = 0; i < PROC_NAME_MAX - 1 && name[i]; i++)
            payload->name[i] = name[i];
        payload->name[i] = '\0';
        req.payload_size = sizeof(proc_set_name_req_t);
        
        int result = _send_and_wait(&req, &resp);
        if (result != EXEC_OK) return result;
        return resp.status;
    }
    
    /* Fallback: direct kernel syscall */
    return syscall3(SYS_PROCESS_SET_NAME, pid, (int)name, (int)type);
}

void libprocess_system_shutdown(void) {
    if (!g_initialized) _libprocess_try_init();
    
    if (g_initialized) {
        exec_request_t req;
        exec_response_t resp;
        exe_memset(&req, 0, sizeof(req));
        
        req.func_id = PROC_OP_SYS_SHUTDOWN;
        req.payload_size = 0;
        
        _send_and_wait(&req, &resp);
        /* If executive handled it, machine should be off already */
    }
    
    /* Fallback: direct kernel syscall */
    syscall0(SYS_SHUTDOWN);
    while (1) { syscall0(SYS_YIELD); }
}

void libprocess_system_restart(void) {
    if (!g_initialized) _libprocess_try_init();
    
    if (g_initialized) {
        exec_request_t req;
        exec_response_t resp;
        exe_memset(&req, 0, sizeof(req));
        
        req.func_id = PROC_OP_SYS_RESTART;
        req.payload_size = 0;
        
        _send_and_wait(&req, &resp);
        /* If executive handled it, machine should be restarting */
    }
    
    /* Fallback: direct kernel syscall */
    syscall0(SYS_RESTART);
    while (1) { syscall0(SYS_YIELD); }
}
