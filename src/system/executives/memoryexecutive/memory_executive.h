/**
 * MaahiOS Memory Executive Header
 * 
 * Description:
 *   Memory Executive manages heap allocation and shared memory services
 *   for user-space processes. Acts as a policy gatekeeper between user
 *   apps and the kernel PMM/paging/SHM managers.
 * 
 *   PID 5 - loaded 4th by sysman (after Log, Cell, Process Executives)
 *   Uses liblog for logging (auto-init)
 *   Uses libcell for cell registration (auto-init)
 *   Direct SYS_MEM_* and SYS_SHM_* syscalls for its own service operations
 *   Dual SHM queues (request + response)
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#ifndef MEMORY_EXECUTIVE_H
#define MEMORY_EXECUTIVE_H

#include "../common/executive_common.h"

/*=============================================================================
 * MEMORY EXECUTIVE OPCODES (starting at EXEC_OP_CUSTOM_BASE = 16)
 *===========================================================================*/

#define MEM_OP_ALLOC_PAGE       (EXEC_OP_CUSTOM_BASE + 0)   /* Allocate 4KB page */
#define MEM_OP_FREE_PAGE        (EXEC_OP_CUSTOM_BASE + 1)   /* Free 4KB page */
#define MEM_OP_ALLOC            (EXEC_OP_CUSTOM_BASE + 2)   /* Allocate memory block */
#define MEM_OP_GET_INFO         (EXEC_OP_CUSTOM_BASE + 3)   /* Get memory info */
#define MEM_OP_SHM_CREATE       (EXEC_OP_CUSTOM_BASE + 4)   /* Create shared memory */
#define MEM_OP_SHM_ATTACH       (EXEC_OP_CUSTOM_BASE + 5)   /* Attach to shared memory */
#define MEM_OP_SHM_DETACH       (EXEC_OP_CUSTOM_BASE + 6)   /* Detach from shared memory */
#define MEM_OP_SHM_DELETE       (EXEC_OP_CUSTOM_BASE + 7)   /* Delete shared memory */

/*=============================================================================
 * MEMORY INFO STRUCTURE (returned to callers)
 *===========================================================================*/

typedef struct {
    uint32_t total_memory;      /* Total system memory (bytes) */
    uint32_t free_memory;       /* Available memory (bytes) */
    uint32_t used_memory;       /* Used memory (bytes) */
} memory_info_t;

/*=============================================================================
 * REQUEST PAYLOADS
 *===========================================================================*/

/* Allocate page request - no payload, result = address */

/* Free page request */
typedef struct {
    uint32_t address;           /* Page address to free */
} mem_free_page_req_t;

/* Allocate memory block request */
typedef struct {
    uint32_t size;              /* Size in bytes */
} mem_alloc_req_t;

/* SHM create request */
typedef struct {
    uint32_t size;              /* Segment size */
} mem_shm_create_req_t;

/* SHM attach request */
typedef struct {
    int32_t shm_id;             /* SHM ID to attach */
} mem_shm_attach_req_t;

/* SHM detach request */
typedef struct {
    uint32_t address;           /* Attached address to detach */
} mem_shm_detach_req_t;

/* SHM delete request */
typedef struct {
    int32_t shm_id;             /* SHM ID to delete */
} mem_shm_delete_req_t;

/*=============================================================================
 * RESPONSE PAYLOADS
 *===========================================================================*/

/* Alloc page response - result field = page address */

/* Alloc memory response - result field = address */

/* SHM create response - result field = shm_id */

/* SHM attach response - result field = address */

/* Get info response */
typedef struct {
    memory_info_t info;
} mem_info_resp_t;

/*=============================================================================
 * EXECUTIVE ENTRY POINT
 *===========================================================================*/

void exe_memory_main(void);

#endif /* MEMORY_EXECUTIVE_H */
