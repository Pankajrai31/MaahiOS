/**
 * MaahiOS Executive Framework - Queue Operations Header
 * 
 * Description:
 *   Provides queue operations for executive request/response handling.
 *   Used by both executives (pop requests) and libraries (push requests).
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#ifndef EXECUTIVE_QUEUE_H
#define EXECUTIVE_QUEUE_H

#include "executive_common.h"

/*=============================================================================
 * SPINLOCK OPERATIONS
 *===========================================================================*/

/**
 * exe_spinlock_acquire - Acquire spinlock
 * @lock: Pointer to lock variable
 */
void exe_spinlock_acquire(volatile uint32_t *lock);

/**
 * exe_spinlock_release - Release spinlock
 * @lock: Pointer to lock variable
 */
void exe_spinlock_release(volatile uint32_t *lock);

/*=============================================================================
 * REQUEST QUEUE OPERATIONS
 *===========================================================================*/

/**
 * exe_request_queue_init - Initialize a request queue
 * @queue: Pointer to queue structure
 */
void exe_request_queue_init(exec_request_queue_t *queue);

/**
 * exe_request_queue_push - Push request to queue (used by libraries)
 * @queue: Pointer to queue
 * @request: Request to push
 * @return: EXEC_OK on success, EXEC_ERR_QUEUE_FULL if full
 */
int exe_request_queue_push(exec_request_queue_t *queue, const exec_request_t *request);

/**
 * exe_request_queue_pop - Pop request from queue (used by executives)
 * @queue: Pointer to queue
 * @request: Output buffer for request
 * @return: EXEC_OK on success, EXEC_ERR_QUEUE_EMPTY if empty
 */
int exe_request_queue_pop(exec_request_queue_t *queue, exec_request_t *request);

/**
 * exe_request_queue_count - Get number of pending requests
 * @queue: Pointer to queue
 * @return: Number of items in queue
 */
int exe_request_queue_count(exec_request_queue_t *queue);

/**
 * exe_request_queue_is_empty - Check if queue is empty
 * @queue: Pointer to queue
 * @return: 1 if empty, 0 if not
 */
int exe_request_queue_is_empty(exec_request_queue_t *queue);

/*=============================================================================
 * RESPONSE QUEUE OPERATIONS
 *===========================================================================*/

/**
 * exe_response_queue_init - Initialize a response queue
 * @queue: Pointer to queue structure
 */
void exe_response_queue_init(exec_response_queue_t *queue);

/**
 * exe_response_queue_push - Push response to queue (used by executives)
 * @queue: Pointer to queue
 * @response: Response to push
 * @return: EXEC_OK on success, EXEC_ERR_QUEUE_FULL if full
 */
int exe_response_queue_push(exec_response_queue_t *queue, const exec_response_t *response);

/**
 * exe_response_queue_pop - Pop response from queue (used by libraries)
 * @queue: Pointer to queue
 * @response: Output buffer for response
 * @return: EXEC_OK on success, EXEC_ERR_QUEUE_EMPTY if empty
 */
int exe_response_queue_pop(exec_response_queue_t *queue, exec_response_t *response);

/**
 * exe_response_queue_pop_by_id - Pop response with specific msg_id
 * @queue: Pointer to queue
 * @msg_id: Message ID to match
 * @response: Output buffer for response
 * @return: EXEC_OK on success, EXEC_ERR_NOT_FOUND if no match
 */
int exe_response_queue_pop_by_id(exec_response_queue_t *queue, uint32_t msg_id, 
                                  exec_response_t *response);

/**
 * exe_response_queue_count - Get number of pending responses
 * @queue: Pointer to queue
 * @return: Number of items in queue
 */
int exe_response_queue_count(exec_response_queue_t *queue);

/*=============================================================================
 * POLL HEARTBEAT HOOK
 *===========================================================================*/

/**
 * Global heartbeat hook — called by every _send_and_wait() poll loop.
 * libwm registers this at init so windowed apps keep their WM heartbeat
 * alive even while blocking on executive IPC.
 * NULL when no windowed system is active (console apps).
 */
extern void (*exe_poll_heartbeat_hook)(void);

/**
 * exe_poll_heartbeat - Call the heartbeat hook if registered.
 * Safe to call from any library's poll loop.
 */
static inline void exe_poll_heartbeat(void) {
    if (exe_poll_heartbeat_hook) exe_poll_heartbeat_hook();
}

/*=============================================================================
 * MEMORY COPY HELPER
 *===========================================================================

/**
 * exe_memcpy - Copy memory
 * @dst: Destination
 * @src: Source
 * @size: Bytes to copy
 */
void exe_memcpy(void *dst, const void *src, uint32_t size);

/**
 * exe_memset - Set memory to value
 * @dst: Destination
 * @val: Value to set
 * @size: Bytes to set
 */
void exe_memset(void *dst, uint8_t val, uint32_t size);

#endif /* EXECUTIVE_QUEUE_H */
