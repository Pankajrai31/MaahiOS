/**
 * MaahiOS Process Executive Implementation
 * 
 * Description:
 *   Process Executive manages user-space process lifecycle.
 *   Makes syscalls to kernel process_manager for actual operations.
 *   Uses liblog for logging (Log Executive must be running first).
 *   Uses libcell for cell registration (Cell Executive must be running first).
 * 
 * PID 4 — loaded 3rd by sysman (after Log Executive PID 2, Cell Executive PID 3)
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#include "process_executive.h"
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

static inline int exe_shm_detach(int shm_id) {
    return syscall1(SYS_SHM_DETACH, shm_id);
}

/*=============================================================================
 * PROCESS KERNEL SYSCALLS (direct — this executive owns process ops)
 *===========================================================================*/

static inline int exe_proc_create(uint32_t entry_point) {
    return syscall1(SYS_PROCESS_CREATE, (int)entry_point);
}

static inline int exe_proc_kill(int32_t pid) {
    return syscall1(SYS_PROCESS_KILL, pid);
}

static inline int exe_proc_info(int32_t pid, void *buffer) {
    return syscall2(SYS_PROCESS_INFO, pid, (int)buffer);
}

static inline int exe_proc_get_count(void) {
    return syscall0(SYS_PROCESS_GET_COUNT);
}

static inline int exe_proc_exec(uint32_t base, uint32_t data, uint32_t size, uint32_t entry_off, uint32_t bss_size) {
    return syscall5(SYS_PROCESS_EXEC, (int)base, (int)data, (int)size, (int)entry_off, (int)bss_size);
}

static inline int exe_mod_copy(int index, uint32_t target) {
    return syscall2(SYS_MOD_COPY, index, (int)target);
}

static inline uint32_t exe_mod_get_addr(int index) {
    return (uint32_t)syscall1(SYS_MOD_GET_ADDR, index);
}

static inline uint32_t exe_mod_get_size(int index) {
    return (uint32_t)syscall1(SYS_MOD_GET_SIZE, index);
}

static inline int exe_proc_list(void *buffer, int max) {
    return syscall2(SYS_PROCESS_LIST, (int)buffer, max);
}

static inline int exe_proc_set_name(int32_t pid, const char *name, uint8_t type) {
    return syscall3(SYS_PROCESS_SET_NAME, pid, (int)name, (int)type);
}

static inline void exe_system_shutdown(void) {
    syscall0(SYS_SHUTDOWN);
}

static inline void exe_system_restart(void) {
    syscall0(SYS_RESTART);
}

/* Standard virtual base for all user processes (per-process page directories) */
#define PROCESS_VIRTUAL_BASE    0x10000000

/* Maximum binary size we'll accept (4MB) */
#define MAX_EXEC_BINARY_SIZE    (4 * 1024 * 1024)

/*=============================================================================
 * EXECUTIVE STATE
 *===========================================================================*/

static exec_control_block_t *g_ecb = NULL;
static exec_request_queue_t *g_req_queue = NULL;
static exec_response_queue_t *g_resp_queue = NULL;
static int g_req_queue_shm_id = -1;
static int g_resp_queue_shm_id = -1;

/*=============================================================================
 * MEMORY MONITORING STATE
 *===========================================================================*/

/* Memory pressure levels: 0=normal, 1=warning, 2=critical */
static int g_memory_pressure = 0;

/* Check memory every N idle cycles (~200 yields ≈ 4 seconds at 50Hz) */
#define MEMORY_CHECK_INTERVAL   200
static uint32_t g_idle_counter = 0;

/* Thresholds (computed from total on first check) */
#define MEM_WARN_DIVISOR        10   /* Warning  if free < total/10 (10%) */
#define MEM_CRITICAL_DIVISOR    20   /* Critical if free < total/20 (5%)  */

/*=============================================================================
 * MEMORY MONITORING HELPERS
 *===========================================================================*/

/**
 * Update active process count in cell registry.
 * Called after every create/kill operation.
 */
static void exe_process_update_count(void) {
    int count = exe_proc_get_count();
    if (count >= 0) {
        libcell_write("system.process.active_count", &count, sizeof(int));
    }
}

/**
 * Query PMM stats via SYS_GET_MEM_INFO and update memory pressure level.
 * Writes stats to cell registry for other components to read.
 */
static void exe_process_check_memory(void) {
    uint32_t info_buf[3] = {0, 0, 0};
    int result = syscall1(SYS_GET_MEM_INFO, (int)info_buf);
    if (result < 0) return;
    
    uint32_t total = info_buf[0];
    uint32_t free  = info_buf[1];
    uint32_t used  = info_buf[2];
    
    if (total == 0) return;
    
    /* Publish stats to cell registry */
    libcell_write("system.memory.total", &total, sizeof(uint32_t));
    libcell_write("system.memory.free",  &free,  sizeof(uint32_t));
    libcell_write("system.memory.used",  &used,  sizeof(uint32_t));
    
    /* Evaluate pressure level */
    uint32_t threshold_warn     = total / MEM_WARN_DIVISOR;
    uint32_t threshold_critical = total / MEM_CRITICAL_DIVISOR;
    
    int prev_pressure = g_memory_pressure;
    
    if (free < threshold_critical) {
        g_memory_pressure = 2;
        if (prev_pressure < 2) {
            liblog(LOG_ERROR, "PROCEXEC", "CRITICAL: Memory nearly exhausted!");
            liblog_hex(LOG_ERROR, "PROCEXEC", "Free bytes:", free);
        }
    } else if (free < threshold_warn) {
        g_memory_pressure = 1;
        if (prev_pressure < 1) {
            liblog(LOG_WARN, "PROCEXEC", "WARNING: Memory pressure detected");
            liblog_hex(LOG_WARN, "PROCEXEC", "Free bytes:", free);
        }
    } else {
        if (prev_pressure > 0) {
            liblog(LOG_INFO, "PROCEXEC", "Memory pressure resolved");
        }
        g_memory_pressure = 0;
    }
}

/*=============================================================================
 * REQUEST HANDLERS
 *===========================================================================*/

static void exe_process_handle_create(const exec_request_t *req, exec_response_t *resp) {
    proc_create_req_t *payload = (proc_create_req_t *)req->payload;
    
    /* Guard: refuse creation if memory is critically low */
    if (g_memory_pressure >= 2) {
        liblog(LOG_ERROR, "PROCEXEC", "Refusing create: memory critical!");
        resp->msg_id = req->msg_id;
        resp->status = EXEC_ERR_NO_MEMORY;
        resp->result = 0;
        resp->payload_size = 0;
        return;
    }
    
    liblog_hex(LOG_INFO, "PROCEXEC", "Create: module_index=", payload->module_index);
    
    /* Get GRUB module address and size */
    uint32_t mod_addr = exe_mod_get_addr((int)payload->module_index);
    if (mod_addr == 0) {
        liblog(LOG_ERROR, "PROCEXEC", "Module not found!");
        resp->msg_id = req->msg_id;
        resp->status = EXEC_ERR_INVALID;
        resp->result = 0;
        resp->payload_size = 0;
        return;
    }
    
    uint32_t mod_size = exe_mod_get_size((int)payload->module_index);
    if (mod_size == 0) {
        liblog(LOG_ERROR, "PROCEXEC", "Module has zero size!");
        resp->msg_id = req->msg_id;
        resp->status = EXEC_ERR_INVALID;
        resp->result = 0;
        resp->payload_size = 0;
        return;
    }
    
    liblog_hex(LOG_INFO, "PROCEXEC", "Module addr:", mod_addr);
    liblog_hex(LOG_INFO, "PROCEXEC", "Module size:", mod_size);
    
    /* Create process with per-process page directory via SYS_PROCESS_EXEC.
     * Binary is mapped at PROCESS_VIRTUAL_BASE in a cloned page directory.
     * bss_size forwarded from caller (Orbit passes it from desktop_app_entry). */
    int pid = exe_proc_exec(PROCESS_VIRTUAL_BASE, mod_addr, mod_size, 0, payload->bss_size);
    
    resp->msg_id = req->msg_id;
    resp->status = (pid >= 0) ? EXEC_OK : pid;
    resp->result = (pid >= 0) ? (uint32_t)pid : 0;
    resp->payload_size = 0;
    
    if (pid >= 0) {
        liblog_hex(LOG_INFO, "PROCEXEC", "Process created, PID:", (uint32_t)pid);
        exe_process_update_count();
    } else {
        liblog(LOG_ERROR, "PROCEXEC", "Failed to create process!");
    }
}

static void exe_process_handle_kill(const exec_request_t *req, exec_response_t *resp) {
    proc_kill_req_t *payload = (proc_kill_req_t *)req->payload;
    
    liblog_hex(LOG_INFO, "PROCEXEC", "Kill: pid=", (uint32_t)payload->pid);
    
    /* Check if target is a system process before attempting kill */
    process_info_t target_info;
    exe_memset(&target_info, 0, sizeof(target_info));
    int info_result = exe_proc_info(payload->pid, &target_info);
    if (info_result < 0) {
        resp->msg_id = req->msg_id;
        resp->status = EXEC_ERR_NOT_FOUND;
        resp->result = 0;
        resp->payload_size = 0;
        return;
    }
    
    if (target_info.type == PROC_TYPE_SYSTEM) {
        liblog(LOG_WARN, "PROCEXEC", "Kill DENIED: target is system process");
        liblog_hex(LOG_WARN, "PROCEXEC", "Protected PID:", (uint32_t)payload->pid);
        resp->msg_id = req->msg_id;
        resp->status = EXEC_ERR_PROTECTED;
        resp->result = 0;
        resp->payload_size = 0;
        return;
    }
    
    int result = exe_proc_kill(payload->pid);
    
    resp->msg_id = req->msg_id;
    resp->status = (result >= 0) ? EXEC_OK : result;
    resp->result = 0;
    resp->payload_size = 0;
    
    if (result >= 0) {
        exe_process_update_count();
    }
}

static void exe_process_handle_get_info(const exec_request_t *req, exec_response_t *resp) {
    proc_info_req_t *payload = (proc_info_req_t *)req->payload;
    proc_info_resp_t *resp_data = (proc_info_resp_t *)resp->payload;
    
    /* Kernel returns 48 bytes: [pid:4][state:4][name:32][type:1][pad:3][mem:4] */
    process_info_t info;
    exe_memset(&info, 0, sizeof(info));
    int result = exe_proc_info(payload->pid, &info);
    
    resp->msg_id = req->msg_id;
    resp->status = (result >= 0) ? EXEC_OK : result;
    resp->result = 0;
    
    if (result >= 0) {
        resp_data->info = info;
        resp->payload_size = sizeof(proc_info_resp_t);
    } else {
        resp->payload_size = 0;
    }
}

static void exe_process_handle_get_count(const exec_request_t *req, exec_response_t *resp) {
    int count = exe_proc_get_count();
    
    resp->msg_id = req->msg_id;
    resp->status = (count >= 0) ? EXEC_OK : count;
    resp->result = (count >= 0) ? (uint32_t)count : 0;
    resp->payload_size = 0;
}

static void exe_process_handle_exec(const exec_request_t *req, exec_response_t *resp) {
    proc_exec_req_t *payload = (proc_exec_req_t *)req->payload;
    
    resp->msg_id = req->msg_id;
    resp->payload_size = 0;
    
    /* Validation: memory pressure */
    if (g_memory_pressure >= 2) {
        liblog(LOG_ERROR, "PROCEXEC", "Refusing exec: memory critical!");
        resp->status = EXEC_ERR_NO_MEMORY;
        resp->result = 0;
        return;
    }
    
    /* Validation: SHM ID and size */
    if (payload->binary_shm_id < 0 || payload->binary_size == 0) {
        liblog(LOG_ERROR, "PROCEXEC", "Exec: invalid SHM ID or zero size");
        resp->status = EXEC_ERR_INVALID;
        resp->result = 0;
        return;
    }
    
    /* Validation: size limit */
    if (payload->binary_size > MAX_EXEC_BINARY_SIZE) {
        liblog(LOG_ERROR, "PROCEXEC", "Exec: binary too large");
        liblog_hex(LOG_ERROR, "PROCEXEC", "Size:", payload->binary_size);
        resp->status = EXEC_ERR_INVALID;
        resp->result = 0;
        return;
    }
    
    /* Validation: base address must be in user range */
    if (payload->base_address < 0x10000000) {
        liblog(LOG_ERROR, "PROCEXEC", "Exec: base address too low");
        liblog_hex(LOG_ERROR, "PROCEXEC", "Base:", payload->base_address);
        resp->status = EXEC_ERR_INVALID;
        resp->result = 0;
        return;
    }
    
    liblog_hex(LOG_INFO, "PROCEXEC", "Exec: base=", payload->base_address);
    liblog_hex(LOG_INFO, "PROCEXEC", "Exec: shm_id=", (uint32_t)payload->binary_shm_id);
    liblog_hex(LOG_INFO, "PROCEXEC", "Exec: size=", payload->binary_size);
    liblog_hex(LOG_INFO, "PROCEXEC", "Exec: entry_off=", payload->entry_offset);
    
    /* Attach caller's SHM to access binary data in our address space.
     * The caller created this SHM, copied the binary into it, and sent
     * us the SHM ID. SHM maps the same physical pages into our page
     * directory, so we can read the binary safely. */
    void *binary_data = exe_shm_attach(payload->binary_shm_id);
    if (!binary_data || (uint32_t)binary_data == 0xFFFFFFFF) {
        liblog(LOG_ERROR, "PROCEXEC", "Exec: failed to attach binary SHM");
        resp->status = EXEC_ERR_NO_MEMORY;
        resp->result = 0;
        return;
    }
    
    /* Call kernel to create process in new address space.
     * binary_data now points to valid memory in our address space. */
    int pid = exe_proc_exec(payload->base_address, (uint32_t)binary_data,
                            payload->binary_size, payload->entry_offset,
                            payload->bss_size);
    
    /* Detach SHM — kernel has already copied the binary into the new
     * process's physical memory, so we no longer need it. The caller
     * will destroy the SHM after receiving our response. */
    exe_shm_detach(payload->binary_shm_id);
    
    resp->status = (pid >= 0) ? EXEC_OK : pid;
    resp->result = (pid >= 0) ? (uint32_t)pid : 0;
    
    if (pid >= 0) {
        liblog_hex(LOG_INFO, "PROCEXEC", "Exec: created PID=", (uint32_t)pid);
        exe_process_update_count();
    } else {
        liblog(LOG_ERROR, "PROCEXEC", "Exec: kernel refused process creation");
    }
}

static void exe_process_handle_ping(const exec_request_t *req, exec_response_t *resp) {
    resp->msg_id = req->msg_id;
    resp->status = EXEC_OK;
    resp->result = 1;
    resp->payload_size = 0;
}

static void exe_process_handle_list(const exec_request_t *req, exec_response_t *resp) {
    resp->msg_id = req->msg_id;
    resp->payload_size = 0;
    
    /* Get process count first to size the SHM */
    int count = exe_proc_get_count();
    if (count <= 0) {
        resp->status = EXEC_OK;
        resp->result = 0;
        return;
    }
    
    /* Each entry is 48 bytes (rich format) */
    uint32_t shm_size = (uint32_t)(count * 48);
    int shm_id = exe_shm_create(shm_size);
    if (shm_id < 0) {
        liblog(LOG_ERROR, "PROCEXEC", "List: failed to create SHM");
        resp->status = EXEC_ERR_NO_MEMORY;
        resp->result = 0;
        return;
    }
    
    void *shm_ptr = exe_shm_attach(shm_id);
    if (!shm_ptr || (uint32_t)shm_ptr == 0xFFFFFFFF) {
        liblog(LOG_ERROR, "PROCEXEC", "List: failed to attach SHM");
        syscall1(SYS_SHM_DESTROY, shm_id);
        resp->status = EXEC_ERR_NO_MEMORY;
        resp->result = 0;
        return;
    }
    
    /* Fill SHM with process list */
    int actual = exe_proc_list(shm_ptr, count);
    exe_shm_detach(shm_id);
    
    if (actual < 0) {
        syscall1(SYS_SHM_DESTROY, shm_id);
        resp->status = EXEC_ERR_INVALID;
        resp->result = 0;
        return;
    }
    
    /* Return SHM ID and count via payload */
    proc_list_resp_t *pl = (proc_list_resp_t *)resp->payload;
    pl->shm_id = shm_id;
    resp->status = EXEC_OK;
    resp->result = (uint32_t)actual;
    resp->payload_size = sizeof(proc_list_resp_t);
}

static void exe_process_handle_shutdown(const exec_request_t *req, exec_response_t *resp) {
    liblog(LOG_WARN, "PROCEXEC", "System shutdown requested!");
    resp->msg_id = req->msg_id;
    resp->status = EXEC_OK;
    resp->result = 0;
    resp->payload_size = 0;
    
    /* Push response before shutting down so caller gets confirmation */
    exe_response_queue_push(g_resp_queue, resp);
    
    /* Brief delay to let response propagate */
    for (int i = 0; i < 5; i++) exe_yield();
    
    /* Issue kernel shutdown */
    exe_system_shutdown();
    /* Never returns */
}

static void exe_process_handle_restart(const exec_request_t *req, exec_response_t *resp) {
    liblog(LOG_WARN, "PROCEXEC", "System restart requested!");
    resp->msg_id = req->msg_id;
    resp->status = EXEC_OK;
    resp->result = 0;
    resp->payload_size = 0;
    
    /* Push response before restarting */
    exe_response_queue_push(g_resp_queue, resp);
    
    /* Brief delay */
    for (int i = 0; i < 5; i++) exe_yield();
    
    /* Issue kernel restart */
    exe_system_restart();
    /* Never returns */
}

/*=============================================================================
 * REQUEST DISPATCHER
 *===========================================================================*/

static void exe_process_dispatch(const exec_request_t *req, exec_response_t *resp) {
    exe_memset(resp, 0, sizeof(exec_response_t));
    
    switch (req->func_id) {
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
            
        case PROC_OP_KILL:
            exe_process_handle_kill(req, resp);
            break;
            
        case PROC_OP_GET_INFO:
            exe_process_handle_get_info(req, resp);
            break;
            
        case PROC_OP_GET_COUNT:
            exe_process_handle_get_count(req, resp);
            break;
            
        case PROC_OP_EXEC:
            exe_process_handle_exec(req, resp);
            break;
            
        case PROC_OP_LIST:
            exe_process_handle_list(req, resp);
            break;
            
        case PROC_OP_SYS_SHUTDOWN:
            exe_process_handle_shutdown(req, resp);
            return;  /* Response already pushed, don't push again */
            
        case PROC_OP_SYS_RESTART:
            exe_process_handle_restart(req, resp);
            return;  /* Response already pushed, don't push again */
            
        case PROC_OP_SET_NAME: {
            proc_set_name_req_t *snp = (proc_set_name_req_t *)req->payload;
            int result = exe_proc_set_name(snp->pid, snp->name, snp->type);
            resp->msg_id = req->msg_id;
            resp->status = (result >= 0) ? EXEC_OK : result;
            resp->result = 0;
            resp->payload_size = 0;
            break;
        }
            
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
    /* liblog auto-inits: if Log Executive is ready, uses SHM queue;
     * if not ready yet, falls back to direct klog syscall. */
    liblog(LOG_INFO, "PROCEXEC", "Process Executive initializing...");
    
    /* Create SHM queues */
    g_req_queue_shm_id = exe_shm_create(sizeof(exec_request_queue_t));
    if (g_req_queue_shm_id < 0) {
        liblog(LOG_ERROR, "PROCEXEC", "Failed to create request queue SHM");
        return -1;
    }
    
    g_req_queue = (exec_request_queue_t *)exe_shm_attach(g_req_queue_shm_id);
    if (!g_req_queue) {
        liblog(LOG_ERROR, "PROCEXEC", "Failed to attach request queue SHM");
        return -1;
    }
    exe_request_queue_init(g_req_queue);
    liblog_hex(LOG_INFO, "PROCEXEC", "Request queue SHM ID:", g_req_queue_shm_id);
    
    g_resp_queue_shm_id = exe_shm_create(sizeof(exec_response_queue_t));
    if (g_resp_queue_shm_id < 0) {
        liblog(LOG_ERROR, "PROCEXEC", "Failed to create response queue SHM");
        return -1;
    }
    
    g_resp_queue = (exec_response_queue_t *)exe_shm_attach(g_resp_queue_shm_id);
    if (!g_resp_queue) {
        liblog(LOG_ERROR, "PROCEXEC", "Failed to attach response queue SHM");
        return -1;
    }
    exe_response_queue_init(g_resp_queue);
    liblog_hex(LOG_INFO, "PROCEXEC", "Response queue SHM ID:", g_resp_queue_shm_id);
    
    /* Set up control block */
    static exec_control_block_t local_ecb;
    g_ecb = &local_ecb;
    exe_str_copy(g_ecb->name, "process_executive", EXEC_NAME_MAX);
    g_ecb->exec_id = EXEC_ID_PROCESS;
    g_ecb->state = EXEC_STATE_STARTING;
    g_ecb->priority = PRIORITY_HIGH;
    g_ecb->request_queue_shm_id = g_req_queue_shm_id;
    g_ecb->response_queue_shm_id = g_resp_queue_shm_id;
    
    /* Write queue SHM IDs to cells for discovery (via libcell — auto-inits) */
    libcell_write("system.exec.process.req_shm", &g_req_queue_shm_id, sizeof(int));
    libcell_write("system.exec.process.resp_shm", &g_resp_queue_shm_id, sizeof(int));
    
    /* Publish initial active process count */
    exe_process_update_count();
    
    /* Run initial memory check to populate cells */
    exe_process_check_memory();
    
    liblog(LOG_INFO, "PROCEXEC", "Process Executive initialized successfully");
    return 0;
}

void exe_process_main(void) {
    if (exe_process_init() != 0) {
        liblog(LOG_ERROR, "PROCEXEC", "Initialization failed! Halting.");
        while (1) __asm__ volatile("hlt");
    }
    
    EXEC_SET_STATE(g_ecb, EXEC_STATE_RUNNING);
    liblog(LOG_INFO, "PROCEXEC", "Entering main loop...");
    
    exec_request_t req;
    exec_response_t resp;
    
    while (!EXEC_SHOULD_STOP(g_ecb)) {
        if (exe_request_queue_pop(g_req_queue, &req) == EXEC_OK) {
            exe_process_dispatch(&req, &resp);
            exe_response_queue_push(g_resp_queue, &resp);
        } else {
            /* Periodic memory monitoring during idle */
            g_idle_counter++;
            if (g_idle_counter >= MEMORY_CHECK_INTERVAL) {
                g_idle_counter = 0;
                exe_process_check_memory();
            }
            exe_yield();
        }
    }
    
    EXEC_SET_STATE(g_ecb, EXEC_STATE_STOPPED);
    liblog(LOG_WARN, "PROCEXEC", "Stopped. Halting.");
    while (1) __asm__ volatile("hlt");
}
