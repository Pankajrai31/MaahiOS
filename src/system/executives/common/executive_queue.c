/**
 * MaahiOS Executive Framework - Queue Operations Implementation
 * 
 * Description:
 *   Implements queue operations for executive request/response handling.
 *   Thread-safe using spinlocks.
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#include "executive_queue.h"

/*=============================================================================
 * MEMORY HELPERS
 *===========================================================================*/

void exe_memcpy(void *dst, const void *src, uint32_t size) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (size--) {
        *d++ = *s++;
    }
}

void exe_memset(void *dst, uint8_t val, uint32_t size) {
    uint8_t *d = (uint8_t *)dst;
    while (size--) {
        *d++ = val;
    }
}

/*=============================================================================
 * SPINLOCK OPERATIONS
 *===========================================================================*/

void exe_spinlock_acquire(volatile uint32_t *lock) {
    /* Simple test-and-set spinlock */
    while (1) {
        /* Try to acquire */
        uint32_t old;
        __asm__ volatile(
            "movl $1, %%eax\n"
            "xchgl %%eax, %1\n"
            "movl %%eax, %0\n"
            : "=r"(old), "+m"(*lock)
            :
            : "eax", "memory"
        );
        
        if (old == 0) {
            /* Acquired successfully */
            return;
        }
        
        /* Spin - could add pause instruction for efficiency */
        __asm__ volatile("pause" ::: "memory");
    }
}

void exe_spinlock_release(volatile uint32_t *lock) {
    /* Memory barrier then release */
    __asm__ volatile("" ::: "memory");
    *lock = 0;
}

/*=============================================================================
 * REQUEST QUEUE OPERATIONS
 *===========================================================================*/

void exe_request_queue_init(exec_request_queue_t *queue) {
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    queue->lock = 0;
    exe_memset(queue->reserved, 0, sizeof(queue->reserved));
    exe_memset(queue->requests, 0, sizeof(queue->requests));
}

int exe_request_queue_push(exec_request_queue_t *queue, const exec_request_t *request) {
    exe_spinlock_acquire(&queue->lock);
    
    if (queue->count >= EXEC_QUEUE_SIZE) {
        exe_spinlock_release(&queue->lock);
        return EXEC_ERR_QUEUE_FULL;
    }
    
    /* Copy request to queue slot */
    exe_memcpy(&queue->requests[queue->tail], request, sizeof(exec_request_t));
    
    /* Update tail and count */
    queue->tail = (queue->tail + 1) % EXEC_QUEUE_SIZE;
    queue->count++;
    
    exe_spinlock_release(&queue->lock);
    return EXEC_OK;
}

int exe_request_queue_pop(exec_request_queue_t *queue, exec_request_t *request) {
    exe_spinlock_acquire(&queue->lock);
    
    if (queue->count == 0) {
        exe_spinlock_release(&queue->lock);
        return EXEC_ERR_QUEUE_EMPTY;
    }
    
    /* Copy request from queue slot */
    exe_memcpy(request, &queue->requests[queue->head], sizeof(exec_request_t));
    
    /* Update head and count */
    queue->head = (queue->head + 1) % EXEC_QUEUE_SIZE;
    queue->count--;
    
    exe_spinlock_release(&queue->lock);
    return EXEC_OK;
}

int exe_request_queue_count(exec_request_queue_t *queue) {
    exe_spinlock_acquire(&queue->lock);
    int count = queue->count;
    exe_spinlock_release(&queue->lock);
    return count;
}

int exe_request_queue_is_empty(exec_request_queue_t *queue) {
    exe_spinlock_acquire(&queue->lock);
    int empty = (queue->count == 0);
    exe_spinlock_release(&queue->lock);
    return empty;
}

/*=============================================================================
 * RESPONSE QUEUE OPERATIONS
 *===========================================================================*/

void exe_response_queue_init(exec_response_queue_t *queue) {
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    queue->lock = 0;
    exe_memset(queue->reserved, 0, sizeof(queue->reserved));
    exe_memset(queue->responses, 0, sizeof(queue->responses));
}

int exe_response_queue_push(exec_response_queue_t *queue, const exec_response_t *response) {
    exe_spinlock_acquire(&queue->lock);
    
    if (queue->count >= EXEC_QUEUE_SIZE) {
        exe_spinlock_release(&queue->lock);
        return EXEC_ERR_QUEUE_FULL;
    }
    
    /* Copy response to queue slot */
    exe_memcpy(&queue->responses[queue->tail], response, sizeof(exec_response_t));
    
    /* Update tail and count */
    queue->tail = (queue->tail + 1) % EXEC_QUEUE_SIZE;
    queue->count++;
    
    exe_spinlock_release(&queue->lock);
    return EXEC_OK;
}

int exe_response_queue_pop(exec_response_queue_t *queue, exec_response_t *response) {
    exe_spinlock_acquire(&queue->lock);
    
    if (queue->count == 0) {
        exe_spinlock_release(&queue->lock);
        return EXEC_ERR_QUEUE_EMPTY;
    }
    
    /* Copy response from queue slot */
    exe_memcpy(response, &queue->responses[queue->head], sizeof(exec_response_t));
    
    /* Update head and count */
    queue->head = (queue->head + 1) % EXEC_QUEUE_SIZE;
    queue->count--;
    
    exe_spinlock_release(&queue->lock);
    return EXEC_OK;
}

int exe_response_queue_pop_by_id(exec_response_queue_t *queue, uint32_t msg_id, 
                                  exec_response_t *response) {
    exe_spinlock_acquire(&queue->lock);
    
    if (queue->count == 0) {
        exe_spinlock_release(&queue->lock);
        return EXEC_ERR_NOT_FOUND;
    }
    
    /* Search for matching msg_id */
    for (uint32_t i = 0; i < queue->count; i++) {
        uint32_t idx = (queue->head + i) % EXEC_QUEUE_SIZE;
        
        if (queue->responses[idx].msg_id == msg_id) {
            /* Found it - copy response */
            exe_memcpy(response, &queue->responses[idx], sizeof(exec_response_t));
            
            /* Remove from queue by shifting remaining items */
            for (uint32_t j = i; j < queue->count - 1; j++) {
                uint32_t curr_idx = (queue->head + j) % EXEC_QUEUE_SIZE;
                uint32_t next_idx = (queue->head + j + 1) % EXEC_QUEUE_SIZE;
                exe_memcpy(&queue->responses[curr_idx], 
                           &queue->responses[next_idx], 
                           sizeof(exec_response_t));
            }
            
            /* Update tail and count */
            queue->tail = (queue->tail - 1 + EXEC_QUEUE_SIZE) % EXEC_QUEUE_SIZE;
            queue->count--;
            
            exe_spinlock_release(&queue->lock);
            return EXEC_OK;
        }
    }
    
    exe_spinlock_release(&queue->lock);
    return EXEC_ERR_NOT_FOUND;
}

int exe_response_queue_count(exec_response_queue_t *queue) {
    exe_spinlock_acquire(&queue->lock);
    int count = queue->count;
    exe_spinlock_release(&queue->lock);
    return count;
}
