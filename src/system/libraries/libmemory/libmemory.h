/**
 * MaahiOS Memory Library - libmemory.h
 * 
 * Description:
 *   User library for memory management.
 *   Auto-initializes on first call (discovers SHM, attaches).
 *   Falls back to direct SYS_MEM_* kernel syscalls if
 *   Memory Executive is not yet running.
 * 
 * Usage:
 *   #include "libmemory.h"
 *   
 *   void *page = libmem_alloc_page();     // just call it
 *   libmem_free_page(page);
 *   void *block = libmem_alloc(1024);
 *   No init() needed - handled automatically.
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
 * libmemory_init - Explicitly initialize memory library (optional)
 * 
 * Auto-called on first use of any libmemory function.
 * Falls back to direct kernel syscalls if executive not ready.
 * 
 * Returns: 0 on success, negative if executive not available
 */
int libmemory_init(void);

/**
 * libmemory_shutdown - Cleanup memory library
 * 
 * Detaches from SHM queues.
 */
void libmemory_shutdown(void);

/*=============================================================================
 * PAGE OPERATIONS
 *===========================================================================*/

/**
 * libmem_alloc_page - Allocate a 4KB page
 * 
 * Returns: Pointer to page on success, NULL on failure
 */
void *libmem_alloc_page(void);

/**
 * libmem_free_page - Free a 4KB page
 * @ptr: Pointer to page
 * 
 * Returns: 0 on success, negative on error
 */
int libmem_free_page(void *ptr);

/*=============================================================================
 * BLOCK ALLOCATION
 *===========================================================================*/

/**
 * libmem_alloc - Allocate a memory block
 * @size: Size in bytes
 * 
 * Returns: Pointer to memory on success, NULL on failure
 */
void *libmem_alloc(uint32_t size);

/*=============================================================================
 * SHARED MEMORY OPERATIONS
 *===========================================================================*/

/**
 * libmem_shm_create - Create shared memory segment
 * @size: Size in bytes
 * 
 * Returns: SHM ID on success, negative on error
 */
int libmem_shm_create(uint32_t size);

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

#endif /* LIBMEMORY_H */
