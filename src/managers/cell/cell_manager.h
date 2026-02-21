/*
 * Cell Manager for MaahiOS
 * 
 * Key-value registry system stored in shared memory
 * - Hierarchical dot-notation keys (e.g., "system.uimanager.window_count")
 * - Spinlock-protected for atomicity
 * - Data stored in SHM for fast userspace access
 */

#ifndef CELL_MANAGER_H
#define CELL_MANAGER_H

#include <stddef.h>

/* Error codes */
#define CELL_OK           0
#define CELL_NOT_FOUND   -1
#define CELL_LOCKED      -2
#define CELL_NO_MEMORY   -3
#define CELL_INVALID_KEY -4
#define CELL_KEY_EXISTS  -5
#define CELL_BUFFER_TOO_SMALL -6

/* Maximum cells and key/value sizes */
#define MAX_CELLS       256
#define MAX_KEY_LEN     128
#define MAX_VALUE_SIZE  256

/**
 * Initialize cell manager
 * Creates SHM region for cell storage
 * Must be called after shm_manager_init()
 * @return SHM ID of cell data region, or negative error code
 */
int cell_manager_init(void);

/**
 * Write a cell (create or update)
 * @param key Dot-notation key (e.g., "system.uimanager.count")
 * @param value Pointer to value data
 * @param size Size of value in bytes
 * @return CELL_OK on success, negative error code on failure
 */
int kernel_cell_write(const char *key, const void *value, size_t size);

/**
 * Read a cell
 * @param key Dot-notation key
 * @param buffer Output buffer for value
 * @param buffer_size Size of output buffer
 * @param actual_size Output: actual size of value (can be NULL)
 * @return CELL_OK on success, negative error code on failure
 */
int kernel_cell_read(const char *key, void *buffer, size_t buffer_size, size_t *actual_size);

/**
 * Delete a cell
 * @param key Dot-notation key
 * @return CELL_OK on success, negative error code on failure
 */
int kernel_cell_delete(const char *key);

/**
 * Check if a cell exists
 * @param key Dot-notation key
 * @return 1 if exists, 0 if not found
 */
int kernel_cell_exists(const char *key);

/**
 * Get SHM ID of cell data region (for userspace attachment)
 * @return SHM ID or negative error code
 */
int kernel_cell_get_shm_id(void);

/**
 * List all cells with a prefix (for debugging/enumeration)
 * @param prefix Key prefix (e.g., "system.uimanager")
 * @param keys Output array of key strings
 * @param max_keys Maximum number of keys to return
 * @return Number of keys found, or negative error code
 */
int kernel_cell_list(const char *prefix, char keys[][MAX_KEY_LEN], int max_keys);

#endif /* CELL_MANAGER_H */
