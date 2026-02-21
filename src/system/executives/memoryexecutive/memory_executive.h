/**
 * MaahiOS Memory Executive Header
 * 
 * Description:
 *   Memory Executive provides heap management services for user processes.
 *   Handles memory allocation, deallocation, and memory info queries.
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#ifndef MEMORY_EXECUTIVE_H
#define MEMORY_EXECUTIVE_H

#include "../common/executive_common.h"

/*=============================================================================
 * MEMORY EXECUTIVE OPCODES
 *===========================================================================*/

#define MEM_OP_ALLOC            (EXEC_OP_CUSTOM_BASE + 0)   /* Allocate memory */
#define MEM_OP_FREE             (EXEC_OP_CUSTOM_BASE + 1)   /* Free memory */
#define MEM_OP_REALLOC          (EXEC_OP_CUSTOM_BASE + 2)   /* Reallocate memory */
#define MEM_OP_GET_INFO         (EXEC_OP_CUSTOM_BASE + 3)   /* Get memory info */
#define MEM_OP_GET_USAGE        (EXEC_OP_CUSTOM_BASE + 4)   /* Get memory usage */
#define MEM_OP_SHM_CREATE       (EXEC_OP_CUSTOM_BASE + 5)   /* Create shared memory */
#define MEM_OP_SHM_ATTACH       (EXEC_OP_CUSTOM_BASE + 6)   /* Attach to shared memory */
#define MEM_OP_SHM_DETACH       (EXEC_OP_CUSTOM_BASE + 7)   /* Detach from shared memory */
#define MEM_OP_SHM_DELETE       (EXEC_OP_CUSTOM_BASE + 8)   /* Delete shared memory */

/*=============================================================================
 * MEMORY INFO STRUCTURE
 *===========================================================================*/

typedef struct {
    uint32_t total_memory;      /* Total system memory */
    uint32_t free_memory;       /* Available memory */
    uint32_t used_memory;       /* Used memory */
    uint32_t heap_size;         /* User heap size */
    uint32_t heap_used;         /* User heap used */
    uint32_t shm_count;         /* Active SHM segments */
    uint32_t shm_total_size;    /* Total SHM size */
} memory_info_t;

/*=============================================================================
 * REQUEST PAYLOADS
 *===========================================================================*/

/* Allocate memory request */
typedef struct {
    uint32_t size;
    uint32_t alignment;     /* 0 = default alignment */
    uint32_t flags;
} mem_alloc_req_t;

/* Free memory request */
typedef struct {
    uint32_t address;
} mem_free_req_t;

/* Realloc request */
typedef struct {
    uint32_t address;
    uint32_t new_size;
} mem_realloc_req_t;

/* SHM create request */
typedef struct {
    uint32_t size;
    uint32_t flags;
    char name[32];
} mem_shm_create_req_t;

/* SHM attach request */
typedef struct {
    int32_t shm_id;
} mem_shm_attach_req_t;

/*=============================================================================
 * RESPONSE PAYLOADS
 *===========================================================================*/

/* Allocate response */
typedef struct {
    uint32_t address;
} mem_alloc_resp_t;

/* Memory info response */
typedef struct {
    memory_info_t info;
} mem_info_resp_t;

/* SHM create response */
typedef struct {
    int32_t shm_id;
} mem_shm_create_resp_t;

/* SHM attach response */
typedef struct {
    uint32_t address;
} mem_shm_attach_resp_t;

/*=============================================================================
 * EXECUTIVE ENTRY POINT
 *===========================================================================*/

void exe_memory_main(void);

#endif /* MEMORY_EXECUTIVE_H */
