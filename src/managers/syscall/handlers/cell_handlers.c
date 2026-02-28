/**
 * Cell (Key-Value Store) Syscall Handlers
 * Domain: 64-79 (cell_write, read, delete, exists, list)
 */

#include "../syscall_manager.h"
#include "../syscall_numbers.h"
#include "../../klog/klog.h"
#include "../../cell/cell_manager.h"
#include <stdint.h>

/* ===========================================================================
 * HANDLERS
 * =========================================================================== */

/**
 * sys_cell_write - Write a cell value
 */
static int sys_cell_write(uint32_t key_ptr, uint32_t value_ptr, uint32_t size,
                          uint32_t arg4, uint32_t arg5) {
    (void)arg4; (void)arg5;
    
    if (!key_ptr || !value_ptr) {
        return CELL_INVALID_KEY;
    }
    
    const char *key = (const char *)key_ptr;
    const void *value = (const void *)value_ptr;
    
    KLOG_DEBUG("SYSCALL", "cell_write: key=%s", key);
    
    /* Call kernel API */
    extern int kernel_cell_write(const char *key, const void *value, size_t size);
    return kernel_cell_write(key, value, (size_t)size);
}

/**
 * sys_cell_read - Read a cell value
 */
static int sys_cell_read(uint32_t key_ptr, uint32_t buffer_ptr, uint32_t buffer_size,
                         uint32_t arg4, uint32_t arg5) {
    (void)arg4; (void)arg5;
    
    if (!key_ptr || !buffer_ptr) {
        return CELL_INVALID_KEY;
    }
    
    const char *key = (const char *)key_ptr;
    void *buffer = (void *)buffer_ptr;
    
    /* Call kernel API — returns bytes read on success, negative on error.
     * Do NOT use arg4 as actual_size_ptr: callers use syscall3 (3 args),
     * so arg4 (from ESI) contains garbage and would corrupt memory. */
    extern int kernel_cell_read(const char *key, void *buffer, size_t buffer_size, size_t *actual_size);
    return kernel_cell_read(key, buffer, (size_t)buffer_size, NULL);
}

/**
 * sys_cell_delete - Delete a cell
 */
static int sys_cell_delete(uint32_t key_ptr, uint32_t arg2, uint32_t arg3,
                           uint32_t arg4, uint32_t arg5) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    if (!key_ptr) {
        return CELL_INVALID_KEY;
    }
    
    const char *key = (const char *)key_ptr;
    
    KLOG_DEBUG("SYSCALL", "cell_delete: key=%s", key);
    
    /* Call kernel API */
    extern int kernel_cell_delete(const char *key);
    return kernel_cell_delete(key);
}

/**
 * sys_cell_exists - Check if cell exists
 */
static int sys_cell_exists(uint32_t key_ptr, uint32_t arg2, uint32_t arg3,
                           uint32_t arg4, uint32_t arg5) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    if (!key_ptr) {
        return 0;
    }
    
    const char *key = (const char *)key_ptr;
    
    /* Call kernel API */
    extern int kernel_cell_exists(const char *key);
    return kernel_cell_exists(key);
}

/**
 * sys_cell_get_shm_id - Get Cell Manager's SHM ID
 */
static int sys_cell_get_shm_id(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                               uint32_t arg4, uint32_t arg5) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    /* Call kernel API */
    extern int kernel_cell_get_shm_id(void);
    return kernel_cell_get_shm_id();
}

/**
 * sys_cell_list - List cells with prefix
 */
static int sys_cell_list(uint32_t prefix_ptr, uint32_t keys_buffer, uint32_t max_keys,
                         uint32_t arg4, uint32_t arg5) {
    (void)arg4; (void)arg5;
    
    if (!prefix_ptr || !keys_buffer || max_keys == 0) {
        return 0;
    }
    
    const char *prefix = (const char *)prefix_ptr;
    
    /* Call kernel API */
    extern int kernel_cell_list(const char *prefix, char keys[][MAX_KEY_LEN], int max_keys);
    return kernel_cell_list(prefix, (char (*)[MAX_KEY_LEN])keys_buffer, (int)max_keys);
}

/* ===========================================================================
 * REGISTRATION
 * =========================================================================== */

void syscall_register_cell_handlers(void) {
    syscall_register(SYS_CELL_WRITE, sys_cell_write);
    syscall_register(SYS_CELL_READ, sys_cell_read);
    syscall_register(SYS_CELL_DELETE, sys_cell_delete);
    syscall_register(SYS_CELL_EXISTS, sys_cell_exists);
    syscall_register(SYS_CELL_GET_SHM_ID, sys_cell_get_shm_id);
    syscall_register(SYS_CELL_LIST, sys_cell_list);
    
    KLOG_DEBUG("SYSCALL", "Cell handlers registered (64-79)");
}
