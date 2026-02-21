/**
 * MaahiOS Process Executive Header
 * 
 * Description:
 *   Process Executive provides process management services.
 *   Handles process creation, termination, and information queries.
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#ifndef PROCESS_EXECUTIVE_H
#define PROCESS_EXECUTIVE_H

#include "../common/executive_common.h"

/*=============================================================================
 * PROCESS EXECUTIVE OPCODES
 *===========================================================================*/

#define PROC_OP_CREATE          (EXEC_OP_CUSTOM_BASE + 0)   /* Create process */
#define PROC_OP_TERMINATE       (EXEC_OP_CUSTOM_BASE + 1)   /* Terminate process */
#define PROC_OP_GET_INFO        (EXEC_OP_CUSTOM_BASE + 2)   /* Get process info */
#define PROC_OP_LIST            (EXEC_OP_CUSTOM_BASE + 3)   /* List processes */
#define PROC_OP_GET_PID         (EXEC_OP_CUSTOM_BASE + 4)   /* Get current PID */
#define PROC_OP_GET_PARENT_PID  (EXEC_OP_CUSTOM_BASE + 5)   /* Get parent PID */
#define PROC_OP_SET_PRIORITY    (EXEC_OP_CUSTOM_BASE + 6)   /* Set priority */
#define PROC_OP_GET_PRIORITY    (EXEC_OP_CUSTOM_BASE + 7)   /* Get priority */
#define PROC_OP_WAIT            (EXEC_OP_CUSTOM_BASE + 8)   /* Wait for process */

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

#define PROC_NAME_MAX       64
#define PROC_MAX_ARGS       256

/*=============================================================================
 * PROCESS INFO STRUCTURE
 *===========================================================================*/

typedef struct {
    uint32_t pid;
    uint32_t parent_pid;
    uint32_t state;         /* 0=ready, 1=running, 2=blocked, 3=terminated */
    uint32_t priority;      /* 0=low, 1=medium, 2=high */
    char name[PROC_NAME_MAX];
    uint32_t start_time;
    uint32_t cpu_time;
    uint32_t memory_used;
} process_info_t;

/*=============================================================================
 * REQUEST PAYLOADS
 *===========================================================================*/

/* Create process request */
typedef struct {
    char name[PROC_NAME_MAX];
    uint32_t module_index;      /* GRUB module index to load */
    uint32_t priority;
    uint32_t flags;
} proc_create_req_t;

/* Terminate process request */
typedef struct {
    uint32_t pid;
    int32_t exit_code;
} proc_terminate_req_t;

/* Get info request */
typedef struct {
    uint32_t pid;
} proc_info_req_t;

/* Set priority request */
typedef struct {
    uint32_t pid;
    uint32_t priority;
} proc_priority_req_t;

/* List processes request */
typedef struct {
    uint32_t offset;
    uint32_t max_count;
} proc_list_req_t;

/* Wait request */
typedef struct {
    uint32_t pid;
    uint32_t timeout_ms;
} proc_wait_req_t;

/*=============================================================================
 * RESPONSE PAYLOADS
 *===========================================================================*/

/* Create process response */
typedef struct {
    uint32_t pid;
} proc_create_resp_t;

/* Get info response */
typedef struct {
    process_info_t info;
} proc_info_resp_t;

/* List processes response */
typedef struct {
    uint32_t total_count;
    uint32_t returned_count;
    process_info_t processes[4];
} proc_list_resp_t;

/* Wait response */
typedef struct {
    int32_t exit_code;
} proc_wait_resp_t;

/*=============================================================================
 * EXECUTIVE ENTRY POINT
 *===========================================================================*/

void exe_process_main(void);

#endif /* PROCESS_EXECUTIVE_H */
