/**
 * MaahiOS Cell Manager
 * Key-value storage using shared memory.
 */

#include "cell_manager.h"
#include "../shm/shm_manager.h"
#include "../klog/klog.h"
#include <stdint.h>

/* ===========================================================================
 * INTERNAL: Configuration & Data Structures
 * =========================================================================== */

typedef struct {
    int used;
    char key[MAX_KEY_LEN];
    unsigned char value[MAX_VALUE_SIZE];
    size_t value_size;
    volatile int lock;
} cell_entry_t;

typedef struct {
    volatile int global_lock;
    int cell_count;
    cell_entry_t cells[MAX_CELLS];
} cell_data_t;

static int cell_shm_id = -1;
static cell_data_t *cell_data = 0;

/* ===========================================================================
 * INTERNAL: Helper Functions
 * =========================================================================== */

static inline void spinlock_acquire(volatile int *lock) {
    while (__sync_lock_test_and_set(lock, 1)) {
        __asm__ volatile("pause");
    }
}

static inline void spinlock_release(volatile int *lock) {
    __sync_lock_release(lock);
}

static int str_len(const char *s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

static int str_cmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a - *b;
}

static void str_copy(char *dst, const char *src, int max_len) {
    int i = 0;
    while (i < max_len - 1 && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static void mem_copy(void *dst, const void *src, size_t n) {
    unsigned char *d = dst;
    const unsigned char *s = src;
    for (size_t i = 0; i < n; i++) { d[i] = s[i]; }
}

/* ===========================================================================
 * INIT: Initialization Functions
 * =========================================================================== */

int cell_manager_init(void) {
    KLOG_INFO("CELL", "Initializing cell manager...");
    
    /* Calculate size needed for cell data */
    size_t cell_data_size = sizeof(cell_data_t);
    KLOG_INFO_HEX("CELL", "Cell data size (bytes): ", cell_data_size);
    
    /* Create SHM region for cell data */
    cell_shm_id = kernel_shm_create(cell_data_size, 0);  /* PID 0 = kernel */
    
    if (cell_shm_id < 0) {
        KLOG_FATAL("CELL", "Failed to create SHM for cell data!");
        return cell_shm_id;
    }
    
    /* Get physical address and map into kernel space */
    unsigned int phys_addr = kernel_shm_get_phys_addr(cell_shm_id);
    if (!phys_addr) {
        KLOG_FATAL("CELL", "Failed to get SHM physical address!");
        return CELL_NO_MEMORY;
    }
    
    /* Kernel can access physical memory directly (identity mapped) */
    cell_data = (cell_data_t *)phys_addr;
    
    /* Initialize cell data structure */
    cell_data->global_lock = 0;
    cell_data->cell_count = 0;
    
    for (int i = 0; i < MAX_CELLS; i++) {
        cell_data->cells[i].used = 0;
        cell_data->cells[i].lock = 0;
        cell_data->cells[i].key[0] = 0;
        cell_data->cells[i].value_size = 0;
    }
    
    KLOG_INFO_HEX2("CELL", "Initialized, SHM ID/cells: ", cell_shm_id, MAX_CELLS);
    
    /* Create root hive structure */
    KLOG_INFO("CELL", "Creating root hive structure...");
    
    int zero = 0;
    int one = 1;
    const char *version = "1.0";
    
    /* System hive - OS-level settings */
    kernel_cell_write("system.version", version, 4);
    kernel_cell_write("system.kernel.initialized", &one, 4);
    
    /* UI hive - UI controls registry */
    kernel_cell_write("ui.controls.count", &zero, 4);
    kernel_cell_write("ui.windows.count", &zero, 4);
    
    /* Process hive - Per-process data */
    kernel_cell_write("process.count", &zero, 4);
    
    /* User hive - User preferences */
    kernel_cell_write("user.initialized", &one, 4);
    
    /* Programs hive - Application registry */
    kernel_cell_write("programs.user.count", &zero, 4);
    kernel_cell_write("programs.system.count", &zero, 4);
    
    KLOG_INFO_HEX("CELL", "Root hives created, cells used: ", cell_data->cell_count);
    
    return cell_shm_id;
}

/* ===========================================================================
 * KERNEL API: Called by syscall handlers
 * =========================================================================== */

int kernel_cell_write(const char *key, const void *value, size_t size) {
    if (!key || !value) {
        return CELL_INVALID_KEY;
    }
    
    if (str_len(key) >= MAX_KEY_LEN) {
        KLOG_ERROR("CELL", "Key too long");
        return CELL_INVALID_KEY;
    }
    
    if (size > MAX_VALUE_SIZE) {
        KLOG_ERROR("CELL", "Value too large");
        return CELL_NO_MEMORY;
    }
    
    if (!cell_data) {
        KLOG_ERROR("CELL", "Cell manager not initialized");
        return CELL_NO_MEMORY;
    }
    
    spinlock_acquire(&cell_data->global_lock);
    
    /* Search for existing cell */
    int slot = -1;
    for (int i = 0; i < MAX_CELLS; i++) {
        if (cell_data->cells[i].used && str_cmp(cell_data->cells[i].key, key) == 0) {
            slot = i;
            break;
        }
    }
    
    /* If not found, find free slot */
    if (slot == -1) {
        for (int i = 0; i < MAX_CELLS; i++) {
            if (!cell_data->cells[i].used) {
                slot = i;
                break;
            }
        }
    }
    
    if (slot == -1) {
        spinlock_release(&cell_data->global_lock);
        KLOG_ERROR("CELL", "No free cell slots");
        return CELL_NO_MEMORY;
    }
    
    cell_entry_t *entry = &cell_data->cells[slot];
    spinlock_acquire(&entry->lock);
    
    /* Write cell data */
    if (!entry->used) {
        str_copy(entry->key, key, MAX_KEY_LEN);
        entry->used = 1;
        cell_data->cell_count++;
    }
    
    mem_copy(entry->value, value, size);
    entry->value_size = size;
    
    spinlock_release(&entry->lock);
    spinlock_release(&cell_data->global_lock);
    
    KLOG_DEBUG_HEX("CELL", "Wrote cell, size: ", size);
    return CELL_OK;
}

int kernel_cell_read(const char *key, void *buffer, size_t buffer_size, size_t *actual_size) {
    if (!key || !buffer) {
        return CELL_INVALID_KEY;
    }
    
    if (!cell_data) {
        KLOG_ERROR("CELL", "Cell manager not initialized");
        return CELL_NO_MEMORY;
    }
    
    spinlock_acquire(&cell_data->global_lock);
    
    /* Search for cell */
    int slot = -1;
    for (int i = 0; i < MAX_CELLS; i++) {
        if (cell_data->cells[i].used && str_cmp(cell_data->cells[i].key, key) == 0) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        spinlock_release(&cell_data->global_lock);
        return CELL_NOT_FOUND;
    }
    
    cell_entry_t *entry = &cell_data->cells[slot];
    spinlock_acquire(&entry->lock);
    spinlock_release(&cell_data->global_lock);
    
    /* Check buffer size */
    if (buffer_size < entry->value_size) {
        spinlock_release(&entry->lock);
        if (actual_size) {
            *actual_size = entry->value_size;
        }
        return CELL_BUFFER_TOO_SMALL;
    }
    
    /* Copy value */
    mem_copy(buffer, entry->value, entry->value_size);
    if (actual_size) {
        *actual_size = entry->value_size;
    }
    
    spinlock_release(&entry->lock);
    
    KLOG_TRACE_HEX("CELL", "Read cell, size: ", entry->value_size);
    return CELL_OK;
}

int kernel_cell_delete(const char *key) {
    if (!key) {
        return CELL_INVALID_KEY;
    }
    
    if (!cell_data) {
        return CELL_NO_MEMORY;
    }
    
    spinlock_acquire(&cell_data->global_lock);
    
    /* Search for cell */
    int slot = -1;
    for (int i = 0; i < MAX_CELLS; i++) {
        if (cell_data->cells[i].used && str_cmp(cell_data->cells[i].key, key) == 0) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        spinlock_release(&cell_data->global_lock);
        return CELL_NOT_FOUND;
    }
    
    cell_entry_t *entry = &cell_data->cells[slot];
    spinlock_acquire(&entry->lock);
    
    /* Mark as unused */
    entry->used = 0;
    entry->key[0] = 0;
    entry->value_size = 0;
    cell_data->cell_count--;
    
    spinlock_release(&entry->lock);
    spinlock_release(&cell_data->global_lock);
    
    KLOG_DEBUG("CELL", "Deleted cell");
    return CELL_OK;
}

/* ===========================================================================
 * KERNEL API: Query Functions
 * =========================================================================== */

int kernel_cell_exists(const char *key) {
    if (!key || !cell_data) {
        return 0;
    }
    
    spinlock_acquire(&cell_data->global_lock);
    
    for (int i = 0; i < MAX_CELLS; i++) {
        if (cell_data->cells[i].used && str_cmp(cell_data->cells[i].key, key) == 0) {
            spinlock_release(&cell_data->global_lock);
            return 1;
        }
    }
    
    spinlock_release(&cell_data->global_lock);
    return 0;
}

int kernel_cell_get_shm_id(void) {
    return cell_shm_id;
}

int kernel_cell_list(const char *prefix, char keys[][MAX_KEY_LEN], int max_keys) {
    if (!prefix || !keys || max_keys <= 0 || !cell_data) {
        return 0;
    }
    
    int prefix_len = str_len(prefix);
    int count = 0;
    
    spinlock_acquire(&cell_data->global_lock);
    
    for (int i = 0; i < MAX_CELLS && count < max_keys; i++) {
        if (!cell_data->cells[i].used) {
            continue;
        }
        
        /* Check if key starts with prefix */
        int match = 1;
        for (int j = 0; j < prefix_len; j++) {
            if (cell_data->cells[i].key[j] != prefix[j]) {
                match = 0;
                break;
            }
        }
        
        if (match) {
            str_copy(keys[count], cell_data->cells[i].key, MAX_KEY_LEN);
            count++;
        }
    }
    
    spinlock_release(&cell_data->global_lock);
    
    return count;
}
