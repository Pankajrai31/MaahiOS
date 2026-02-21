/**
 * MaahiOS Memory Library - libmemory.h
 * 
 * Description:
 *   User library for memory management.
 *   Applications include this header for heap allocation and SHM.
 *   Internally communicates with Memory Executive via SHM queues.
 * 
 * Usage:
 *   #include <libmemory.h>
 *   
 *   libmemory_init();
 *   void *ptr = libmem_alloc(1024);
 *   libmem_free(ptr);
 *   libmemory_shutdown();
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#ifndef LIBMEMORY_H
#define LIBMEMORY_H

#include <stdint.h>
#include "../../executives/memoryexecutive/memory_executive.h"

/*=============================================================================
 * LIBRARY INITIALIZATION
 *===========================================================================*/

/**
 * libmemory_init - Initialize memory library
 * 
 * Connects to Memory Executive's SHM queues.
 * Must be called before any other libmemory functions.
 * 
 * Returns: 0 on success, negative on error
 */
int libmemory_init(void);

/**
 * libmemory_shutdown - Cleanup memory library
 */
void libmemory_shutdown(void);

/*=============================================================================
 * HEAP OPERATIONS
 *===========================================================================*/

/**
 * libmem_alloc - Allocate memory
 * @size: Size in bytes
 * 
 * Returns: Pointer to memory on success, NULL on failure
 */
void *libmem_alloc(uint32_t size);

/**
 * libmem_alloc_aligned - Allocate aligned memory
 * @size: Size in bytes
 * @alignment: Alignment (must be power of 2)
 * 
 * Returns: Pointer to memory on success, NULL on failure
 */
void *libmem_alloc_aligned(uint32_t size, uint32_t alignment);

/**
 * libmem_free - Free memory
 * @ptr: Pointer to memory
 * 
 * Returns: 0 on success, negative on error
 */
int libmem_free(void *ptr);

/**
 * libmem_realloc - Reallocate memory
 * @ptr: Pointer to existing memory
 * @new_size: New size in bytes
 * 
 * Returns: Pointer to memory on success, NULL on failure
 */
void *libmem_realloc(void *ptr, uint32_t new_size);

/*=============================================================================
 * SHARED MEMORY OPERATIONS
 *===========================================================================*/

/**
 * libmem_shm_create - Create shared memory segment
 * @name: Segment name (for lookup)
 * @size: Size in bytes
 * @flags: Creation flags
 * 
 * Returns: SHM ID on success, negative on error
 */
int libmem_shm_create(const char *name, uint32_t size, uint32_t flags);

/**
 * libmem_shm_attach - Attach to shared memory segment
 * @shm_id: SHM ID
 * 
 * Returns: Pointer to memory on success, NULL on failure
 */
void *libmem_shm_attach(int shm_id);

/**
 * libmem_shm_detach - Detach from shared memory segment
 * @ptr: Pointer to attached memory
 * 
 * Returns: 0 on success, negative on error
 */
int libmem_shm_detach(void *ptr);

/**
 * libmem_shm_delete - Delete shared memory segment
 * @shm_id: SHM ID
 * 
 * Returns: 0 on success, negative on error
 */
int libmem_shm_delete(int shm_id);

/*=============================================================================
 * MEMORY INFO
 *===========================================================================*/

/**
 * libmem_get_info - Get memory information
 * @info: Output structure
 * 
 * Returns: 0 on success, negative on error
 */
int libmem_get_info(memory_info_t *info);

/**
 * libmem_get_usage - Get current process memory usage
 * 
 * Returns: Bytes used on success, negative on error
 */
int libmem_get_usage(void);

#endif /* LIBMEMORY_H */
