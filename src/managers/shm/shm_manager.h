/*
 * Shared Memory Manager for MaahiOS
 * 
 * Manages shared memory regions between processes
 * - Physical pages allocated from PMM
 * - Mapped into multiple process address spaces
 * - Used for IPC and data structures like Cell Manager
 */

#ifndef SHM_MANAGER_H
#define SHM_MANAGER_H

#include <stddef.h>

/* Error codes */
#define SHM_OK           0
#define SHM_NO_MEMORY   -1
#define SHM_INVALID_ID  -2
#define SHM_NOT_FOUND   -3
#define SHM_ALREADY_ATTACHED -4
#define SHM_NOT_ATTACHED -5

/* Maximum shared memory regions */
#define MAX_SHM_REGIONS 64

/* Virtual address spacing per SHM slot (4 MB each, supports surfaces up to 4 MB) */
#define SHM_SLOT_VIRT_SIZE  0x400000
/* Base virtual address for SHM mappings */
#define SHM_VIRT_BASE       0x80000000

/* SHM region info (for userspace queries) */
typedef struct {
    int shm_id;
    unsigned int size;
    int owner_pid;
    int attached_count;
} shm_info_t;

/**
 * Initialize shared memory manager
 * Must be called after PMM and paging init
 * Returns: 0 on success, -1 on failure
 */
int shm_manager_init(void);

/**
 * Create a new shared memory region
 * @param size Size in bytes (rounded up to page boundary)
 * @param owner_pid Process creating the SHM
 * @return SHM ID (>=0) on success, negative error code on failure
 */
int kernel_shm_create(size_t size, int owner_pid);

/**
 * Attach shared memory to a process
 * @param shm_id Shared memory ID
 * @param pid Process to attach to
 * @param virt_addr Desired virtual address (or 0 for automatic)
 * @return Virtual address in process space, or 0 on error
 */
unsigned int kernel_shm_attach(int shm_id, int pid, unsigned int virt_addr);

/**
 * Detach shared memory from a process
 * @param shm_id Shared memory ID
 * @param pid Process to detach from
 * @return SHM_OK on success, negative error code on failure
 */
int kernel_shm_detach(int shm_id, int pid);

/**
 * Destroy a shared memory region (only if no attachments)
 * @param shm_id Shared memory ID
 * @return SHM_OK on success, negative error code on failure
 */
int kernel_shm_destroy(int shm_id);

/**
 * Get physical address of SHM region (for kernel use)
 * @param shm_id Shared memory ID
 * @return Physical address or 0 on error
 */
unsigned int kernel_shm_get_phys_addr(int shm_id);

/**
 * Get info about SHM region
 * @param shm_id Shared memory ID
 * @param info Output structure
 * @return SHM_OK on success, negative error code on failure
 */
int kernel_shm_get_info(int shm_id, shm_info_t *info);

#endif /* SHM_MANAGER_H */
