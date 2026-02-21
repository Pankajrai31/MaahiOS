/**
 * MaahiOS Queue Library - libqueue.h
 * 
 * Description:
 *   User library for message queue operations.
 *   Applications include this header for IPC via message queues.
 *   Internally communicates with Queue Executive via SHM queues.
 * 
 * Usage:
 *   #include <libqueue.h>
 *   
 *   libqueue_init();
 *   int qid = libqueue_create("app.messages", 32, 256, 0);
 *   libqueue_send(qid, message, sizeof(message));
 *   libqueue_receive(qid, buffer, sizeof(buffer), 0);
 *   libqueue_shutdown();
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#ifndef LIBQUEUE_H
#define LIBQUEUE_H

#include <stdint.h>
#include "../../executives/queueexecutive/queue_executive.h"

/*=============================================================================
 * LIBRARY INITIALIZATION
 *===========================================================================*/

/**
 * libqueue_init - Initialize queue library
 * 
 * Connects to Queue Executive's SHM queues.
 * Must be called before any other libqueue functions.
 * 
 * Returns: 0 on success, negative on error
 */
int libqueue_init(void);

/**
 * libqueue_shutdown - Cleanup queue library
 */
void libqueue_shutdown(void);

/*=============================================================================
 * QUEUE OPERATIONS
 *===========================================================================*/

/**
 * libqueue_create - Create a message queue
 * @name: Queue name
 * @max_messages: Maximum messages in queue
 * @message_size: Maximum size of each message
 * @flags: Creation flags
 * 
 * Returns: Queue ID on success, negative on error
 */
int libqueue_create(const char *name, uint32_t max_messages, 
                    uint32_t message_size, uint32_t flags);

/**
 * libqueue_delete - Delete a message queue
 * @queue_id: Queue ID
 * 
 * Returns: 0 on success, negative on error
 */
int libqueue_delete(int queue_id);

/**
 * libqueue_send - Send message to queue
 * @queue_id: Queue ID
 * @data: Message data
 * @size: Message size
 * 
 * Returns: 0 on success, negative on error
 */
int libqueue_send(int queue_id, const void *data, uint32_t size);

/**
 * libqueue_receive - Receive message from queue
 * @queue_id: Queue ID
 * @buffer: Buffer to receive message
 * @max_size: Maximum bytes to receive
 * @timeout_ms: Timeout (0 = no wait, -1 = forever)
 * 
 * Returns: Bytes received on success, negative on error/timeout
 */
int libqueue_receive(int queue_id, void *buffer, uint32_t max_size, uint32_t timeout_ms);

/**
 * libqueue_peek - Peek at next message without removing
 * @queue_id: Queue ID
 * @buffer: Buffer to receive message
 * @max_size: Maximum bytes to receive
 * 
 * Returns: Bytes received on success, negative on error
 */
int libqueue_peek(int queue_id, void *buffer, uint32_t max_size);

/**
 * libqueue_get_info - Get queue information
 * @queue_id: Queue ID
 * @info: Output structure
 * 
 * Returns: 0 on success, negative on error
 */
int libqueue_get_info(int queue_id, queue_info_t *info);

/**
 * libqueue_flush - Flush all messages from queue
 * @queue_id: Queue ID
 * 
 * Returns: Number of messages flushed on success, negative on error
 */
int libqueue_flush(int queue_id);

#endif /* LIBQUEUE_H */
