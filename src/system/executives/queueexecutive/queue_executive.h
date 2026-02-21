/**
 * MaahiOS Queue Executive Header
 * 
 * Description:
 *   Queue Executive manages message queues for inter-process communication.
 *   It creates and manages queues that other executives and apps can use.
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#ifndef QUEUE_EXECUTIVE_H
#define QUEUE_EXECUTIVE_H

#include "../common/executive_common.h"

/*=============================================================================
 * QUEUE EXECUTIVE OPCODES
 *===========================================================================*/

#define QUEUE_OP_CREATE         (EXEC_OP_CUSTOM_BASE + 0)   /* Create a queue */
#define QUEUE_OP_DELETE         (EXEC_OP_CUSTOM_BASE + 1)   /* Delete a queue */
#define QUEUE_OP_SEND           (EXEC_OP_CUSTOM_BASE + 2)   /* Send message to queue */
#define QUEUE_OP_RECEIVE        (EXEC_OP_CUSTOM_BASE + 3)   /* Receive message from queue */
#define QUEUE_OP_PEEK           (EXEC_OP_CUSTOM_BASE + 4)   /* Peek at next message */
#define QUEUE_OP_GET_INFO       (EXEC_OP_CUSTOM_BASE + 5)   /* Get queue info */
#define QUEUE_OP_LIST           (EXEC_OP_CUSTOM_BASE + 6)   /* List all queues */
#define QUEUE_OP_FLUSH          (EXEC_OP_CUSTOM_BASE + 7)   /* Flush queue */

/*=============================================================================
 * QUEUE CONFIGURATION
 *===========================================================================*/

#define QUEUE_NAME_MAX          32
#define QUEUE_MSG_MAX_SIZE      256
#define QUEUE_MAX_MESSAGES      64
#define MAX_QUEUES              32

/*=============================================================================
 * QUEUE INFO STRUCTURE
 *===========================================================================*/

typedef struct {
    char name[QUEUE_NAME_MAX];
    uint32_t queue_id;
    uint32_t owner_pid;
    uint32_t message_count;
    uint32_t max_messages;
    uint32_t message_size;
    uint32_t flags;
} queue_info_t;

/*=============================================================================
 * REQUEST PAYLOADS
 *===========================================================================*/

/* Create queue request */
typedef struct {
    char name[QUEUE_NAME_MAX];
    uint32_t max_messages;
    uint32_t message_size;
    uint32_t flags;
} queue_create_req_t;

/* Delete queue request */
typedef struct {
    uint32_t queue_id;
} queue_delete_req_t;

/* Send message request */
typedef struct {
    uint32_t queue_id;
    uint32_t size;
    uint8_t data[QUEUE_MSG_MAX_SIZE];
} queue_send_req_t;

/* Receive message request */
typedef struct {
    uint32_t queue_id;
    uint32_t timeout_ms;    /* 0 = no wait, -1 = forever */
} queue_receive_req_t;

/*=============================================================================
 * RESPONSE PAYLOADS
 *===========================================================================*/

/* Create queue response */
typedef struct {
    uint32_t queue_id;
} queue_create_resp_t;

/* Receive message response */
typedef struct {
    uint32_t size;
    uint8_t data[QUEUE_MSG_MAX_SIZE];
} queue_receive_resp_t;

/* Get info response */
typedef struct {
    queue_info_t info;
} queue_info_resp_t;

/*=============================================================================
 * EXECUTIVE ENTRY POINT
 *===========================================================================*/

void exe_queue_main(void);

#endif /* QUEUE_EXECUTIVE_H */
