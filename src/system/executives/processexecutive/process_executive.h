/**
 * MaahiOS Process Executive Header
 * 
 * Description:
 *   Process Executive manages user-space process lifecycle — creating,
 *   terminating, and querying processes. Acts as a policy gatekeeper
 *   between user apps and the kernel process_manager.
 * 
 *   PID 4 — loaded 3rd by sysman (after Log Executive, Cell Executive)
 *   Uses liblog for logging (auto-init)
 *   Uses libcell for cell registration (auto-init)
 *   Direct SYS_PROCESS_* syscalls for its own service operations
 *   Dual SHM queues (request + response)
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#ifndef PROCESS_EXECUTIVE_H
#define PROCESS_EXECUTIVE_H

#include "../common/executive_common.h"

/*=============================================================================
 * PROCESS EXECUTIVE OPCODES (starting at EXEC_OP_CUSTOM_BASE = 16)
 *===========================================================================*/

#define PROC_OP_CREATE      (EXEC_OP_CUSTOM_BASE + 0)   /* Create a process */
#define PROC_OP_KILL        (EXEC_OP_CUSTOM_BASE + 1)   /* Kill a process */
#define PROC_OP_GET_INFO    (EXEC_OP_CUSTOM_BASE + 2)   /* Get process info */
#define PROC_OP_GET_COUNT   (EXEC_OP_CUSTOM_BASE + 3)   /* Get process count */
#define PROC_OP_EXEC        (EXEC_OP_CUSTOM_BASE + 4)   /* Exec binary in new address space */
#define PROC_OP_LIST        (EXEC_OP_CUSTOM_BASE + 5)   /* List all active processes */
#define PROC_OP_SYS_SHUTDOWN (EXEC_OP_CUSTOM_BASE + 6)  /* System power off */
#define PROC_OP_SYS_RESTART  (EXEC_OP_CUSTOM_BASE + 7)  /* System restart */

/*=============================================================================
 * PROCESS INFO STRUCTURE (returned to callers)
 *===========================================================================*/

typedef struct {
    int32_t  pid;
    uint32_t state;         /* PROCESS_STATE_READY=1, RUNNING=2 */
} process_info_t;

/*=============================================================================
 * REQUEST PAYLOADS
 *===========================================================================*/

/* Create process request */
typedef struct {
    uint32_t module_index;      /* GRUB module index to load from */
    uint32_t load_address;      /* Target address to copy module to */
} proc_create_req_t;

/* Exec binary request (load .mex app in new address space) */
typedef struct {
    uint32_t base_address;      /* Virtual load address (e.g. 0x10000000) */
    int32_t  binary_shm_id;     /* SHM region containing binary data */
    uint32_t binary_size;       /* Size of binary in bytes */
    uint32_t entry_offset;      /* Entry point offset from base */
} proc_exec_req_t;

/* Kill process request */
typedef struct {
    int32_t pid;                /* PID of process to terminate */
} proc_kill_req_t;

/* Get info request */
typedef struct {
    int32_t pid;                /* PID to query */
} proc_info_req_t;

/* Get count request — no payload needed */

/*=============================================================================
 * RESPONSE PAYLOADS
 *===========================================================================*/

/* Create process response — result field = new PID */

/* Get info response */
typedef struct {
    process_info_t info;
} proc_info_resp_t;

/* Get count response — result field = count */

/* List processes response — SHM-based */
typedef struct {
    int32_t shm_id;             /* SHM ID containing process_info_t array */
} proc_list_resp_t;

/* Shutdown/Restart requests — no payload needed */

/*=============================================================================
 * EXECUTIVE ENTRY POINT
 *===========================================================================*/

void exe_process_main(void);

#endif /* PROCESS_EXECUTIVE_H */
