/**
 * MaahiOS Shared Memory Manager
 * Inter-process shared memory regions.
 */

#include "shm_manager.h"
#include "../klog/klog.h"
#include "../memory/paging.h"
#include "../memory/pmm.h"
#include "../process/process_manager.h"
#include <stdint.h>

/* ===========================================================================
 * INTERNAL: Configuration & Data Structures
 * =========================================================================== */

/* Maximum attachments per SHM region */
#define MAX_SHM_ATTACHMENTS 32

typedef struct {
    int used;
    int shm_id;
    unsigned int phys_addr;
    unsigned int size;
    int owner_pid;
    int attached_pids[MAX_SHM_ATTACHMENTS];
    unsigned int virt_addrs[MAX_SHM_ATTACHMENTS];
    int attached_count;
    volatile int lock;
} shm_region_t;

static shm_region_t shm_table[MAX_SHM_REGIONS];
static int next_shm_id = 1;
static volatile int shm_global_lock = 0;

/* ===========================================================================
 * INTERNAL: Spinlock Helpers
 * =========================================================================== */

static inline void spinlock_acquire(volatile int *lock) {
    while (__sync_lock_test_and_set(lock, 1)) {
        __asm__ volatile("pause");
    }
}

static inline void spinlock_release(volatile int *lock) {
    __sync_lock_release(lock);
}

/* ===========================================================================
 * INIT: Initialization Functions
 * =========================================================================== */

int shm_manager_init(void) {
    KLOG_INFO("SHM", "Initializing shared memory manager...");
    
    /* Clear SHM table */
    for (int i = 0; i < MAX_SHM_REGIONS; i++) {
        shm_table[i].used = 0;
        shm_table[i].lock = 0;
        shm_table[i].attached_count = 0;
        for (int j = 0; j < MAX_SHM_ATTACHMENTS; j++) {
            shm_table[i].attached_pids[j] = -1;
            shm_table[i].virt_addrs[j] = 0;
        }
    }
    
    KLOG_INFO_HEX("SHM", "Manager initialized, max regions: ", MAX_SHM_REGIONS);
    return 0;  /* Success */
}

/* ===========================================================================
 * KERNEL API: Create/Destroy SHM Regions
 * =========================================================================== */

int kernel_shm_create(size_t size, int owner_pid) {
    if (size == 0) {
        KLOG_ERROR("SHM", "Cannot create SHM with size 0");
        return SHM_NO_MEMORY;
    }
    
    /* Round up to page boundary */
    size = (size + 4095) & ~4095;
    
    spinlock_acquire(&shm_global_lock);
    
    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < MAX_SHM_REGIONS; i++) {
        if (!shm_table[i].used) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        spinlock_release(&shm_global_lock);
        KLOG_ERROR("SHM", "No free SHM slots");
        return SHM_NO_MEMORY;
    }
    
    /* Allocate contiguous physical pages */
    unsigned int phys_addr = (unsigned int)pmm_alloc_size(size);
    
    if (!phys_addr) {
        spinlock_release(&shm_global_lock);
        KLOG_ERROR("SHM", "Failed to allocate physical pages");
        return SHM_NO_MEMORY;
    }
    
    /* Initialize SHM descriptor */
    shm_table[slot].used = 1;
    shm_table[slot].shm_id = next_shm_id++;
    shm_table[slot].phys_addr = phys_addr;
    shm_table[slot].size = size;
    shm_table[slot].owner_pid = owner_pid;
    shm_table[slot].attached_count = 0;
    shm_table[slot].lock = 0;
    
    for (int i = 0; i < MAX_SHM_ATTACHMENTS; i++) {
        shm_table[slot].attached_pids[i] = -1;
        shm_table[slot].virt_addrs[i] = 0;
    }
    
    int shm_id = shm_table[slot].shm_id;
    spinlock_release(&shm_global_lock);
    
    KLOG_INFO_HEX2("SHM", "Created region, ID/size: ", shm_id, size);
    return shm_id;
}

/* ===========================================================================
 * KERNEL API: Attach/Detach SHM Regions
 * =========================================================================== */

unsigned int kernel_shm_attach(int shm_id, int pid, unsigned int virt_addr) {
    spinlock_acquire(&shm_global_lock);
    
    int slot = -1;
    for (int i = 0; i < MAX_SHM_REGIONS; i++) {
        if (shm_table[i].used && shm_table[i].shm_id == shm_id) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        spinlock_release(&shm_global_lock);
        KLOG_ERROR_HEX("SHM", "Attach: SHM not found, ID: ", shm_id);
        return 0;
    }
    
    shm_region_t *region = &shm_table[slot];
    spinlock_acquire(&region->lock);
    
    for (int i = 0; i < MAX_SHM_ATTACHMENTS; i++) {
        if (region->attached_pids[i] == pid) {
            spinlock_release(&region->lock);
            spinlock_release(&shm_global_lock);
            KLOG_WARN_HEX2("SHM", "Already attached, PID/SHM: ", pid, shm_id);
            return region->virt_addrs[i];
        }
    }
    
    int attach_slot = -1;
    for (int i = 0; i < MAX_SHM_ATTACHMENTS; i++) {
        if (region->attached_pids[i] == -1) {
            attach_slot = i;
            break;
        }
    }
    
    if (attach_slot == -1) {
        spinlock_release(&region->lock);
        spinlock_release(&shm_global_lock);
        KLOG_ERROR_HEX("SHM", "Too many attachments for SHM: ", shm_id);
        return 0;
    }
    
    if (virt_addr == 0) {
        /* Use table slot index (0..63) with 4 MB spacing so large SHMs
         * (e.g. 2 MB window surfaces) never overlap each other. */
        virt_addr = SHM_VIRT_BASE + ((unsigned int)slot * SHM_SLOT_VIRT_SIZE);
    }
    
    /* Map SHM pages into the process's page directory.
     * For processes using kernel_page_directory (executives), this maps
     * into the shared address space. For .mex apps with their own page
     * directory, this maps into their private address space. */
    extern uint32_t *kernel_page_directory;
    uint32_t *target_dir = kernel_page_directory;  /* Default fallback */
    
    if (pid > 0) {
        process_t *proc = process_get_by_pid(pid);
        if (proc && proc->page_directory) {
            target_dir = proc->page_directory;
        }
    }
    
    unsigned int pages = region->size / 4096;
    for (unsigned int i = 0; i < pages; i++) {
        unsigned int phys = region->phys_addr + (i * 4096);
        unsigned int virt = virt_addr + (i * 4096);
        paging_map_page(target_dir, virt, phys, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    }
    
    region->attached_pids[attach_slot] = pid;
    region->virt_addrs[attach_slot] = virt_addr;
    region->attached_count++;
    
    spinlock_release(&region->lock);
    spinlock_release(&shm_global_lock);
    
    KLOG_INFO_HEX2("SHM", "Attached to PID, virt addr: ", pid, virt_addr);
    return virt_addr;
}

int kernel_shm_detach(int shm_id, int pid) {
    spinlock_acquire(&shm_global_lock);
    
    /* Find SHM region */
    int slot = -1;
    for (int i = 0; i < MAX_SHM_REGIONS; i++) {
        if (shm_table[i].used && shm_table[i].shm_id == shm_id) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        spinlock_release(&shm_global_lock);
        return SHM_NOT_FOUND;
    }
    
    shm_region_t *region = &shm_table[slot];
    spinlock_acquire(&region->lock);
    
    /* Find attachment */
    int attach_slot = -1;
    for (int i = 0; i < MAX_SHM_ATTACHMENTS; i++) {
        if (region->attached_pids[i] == pid) {
            attach_slot = i;
            break;
        }
    }
    
    if (attach_slot == -1) {
        spinlock_release(&region->lock);
        spinlock_release(&shm_global_lock);
        return SHM_NOT_ATTACHED;
    }
    
    /* Unmap pages from process */
    unsigned int virt_addr = region->virt_addrs[attach_slot];
    unsigned int pages = region->size / 4096;
    
    /* Unmap pages from the process's page directory */
    extern uint32_t *kernel_page_directory;
    uint32_t *target_dir = kernel_page_directory;
    
    if (pid > 0) {
        process_t *proc = process_get_by_pid(pid);
        if (proc && proc->page_directory) {
            target_dir = proc->page_directory;
        }
    }
    
    for (unsigned int i = 0; i < pages; i++) {
        unsigned int virt = virt_addr + (i * 4096);
        unsigned int pd_index = virt >> 22;
        unsigned int pt_index = (virt >> 12) & 0x3FF;
        
        if (target_dir[pd_index] & PAGE_PRESENT) {
            uint32_t *page_table = (uint32_t *)(target_dir[pd_index] & 0xFFFFF000);
            if (page_table) {
                page_table[pt_index] = 0;
            }
        }
    }
    
    /* Flush TLB */
    __asm__ volatile("mov %%cr3, %%eax; mov %%eax, %%cr3" ::: "eax");
    
    /* Remove attachment */
    region->attached_pids[attach_slot] = -1;
    region->virt_addrs[attach_slot] = 0;
    region->attached_count--;
    
    spinlock_release(&region->lock);
    spinlock_release(&shm_global_lock);
    
    KLOG_INFO_HEX2("SHM", "Detached from PID/SHM: ", pid, shm_id);
    return SHM_OK;
}

int kernel_shm_destroy(int shm_id) {
    spinlock_acquire(&shm_global_lock);
    
    /* Find SHM region */
    int slot = -1;
    for (int i = 0; i < MAX_SHM_REGIONS; i++) {
        if (shm_table[i].used && shm_table[i].shm_id == shm_id) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        spinlock_release(&shm_global_lock);
        return SHM_NOT_FOUND;
    }
    
    shm_region_t *region = &shm_table[slot];
    
    /* Check if still attached */
    if (region->attached_count > 0) {
        spinlock_release(&shm_global_lock);
        KLOG_WARN_HEX("SHM", "Cannot destroy, still attached: ", shm_id);
        return SHM_INVALID_ID;
    }
    
    /* Free physical pages */
    unsigned int pages = region->size / 4096;
    extern void pmm_free_page(void *addr);
    for (unsigned int i = 0; i < pages; i++) {
        pmm_free_page((void *)(region->phys_addr + (i * 4096)));
    }
    
    /* Mark slot as free */
    region->used = 0;
    
    spinlock_release(&shm_global_lock);
    
    KLOG_INFO_HEX("SHM", "Destroyed region: ", shm_id);
    return SHM_OK;
}

/* ===========================================================================
 * KERNEL API: Process Cleanup
 * =========================================================================== */

/**
 * Detach a process from ALL shared memory regions.
 * Called on process exit to prevent SHM attachment slot leaks.
 * We skip unmapping pages because the process is being terminated
 * and its page directory will be abandoned.
 */
void kernel_shm_cleanup_process(int pid) {
    spinlock_acquire(&shm_global_lock);

    int cleaned = 0;
    for (int i = 0; i < MAX_SHM_REGIONS; i++) {
        if (!shm_table[i].used) continue;

        shm_region_t *region = &shm_table[i];
        for (int j = 0; j < MAX_SHM_ATTACHMENTS; j++) {
            if (region->attached_pids[j] == pid) {
                region->attached_pids[j] = -1;
                region->virt_addrs[j] = 0;
                region->attached_count--;
                cleaned++;
                break; /* Each PID appears at most once per region */
            }
        }
    }

    spinlock_release(&shm_global_lock);

    if (cleaned > 0) {
        KLOG_INFO_HEX2("SHM", "Cleaned up PID, regions: ", pid, cleaned);
    }
}

/* ===========================================================================
 * KERNEL API: SHM Information Functions
 * =========================================================================== */

unsigned int kernel_shm_get_phys_addr(int shm_id) {
    for (int i = 0; i < MAX_SHM_REGIONS; i++) {
        if (shm_table[i].used && shm_table[i].shm_id == shm_id) {
            return shm_table[i].phys_addr;
        }
    }
    return 0;
}

int kernel_shm_get_info(int shm_id, shm_info_t *info) {
    if (!info) return SHM_INVALID_ID;
    
    for (int i = 0; i < MAX_SHM_REGIONS; i++) {
        if (shm_table[i].used && shm_table[i].shm_id == shm_id) {
            info->shm_id = shm_table[i].shm_id;
            info->size = shm_table[i].size;
            info->owner_pid = shm_table[i].owner_pid;
            info->attached_count = shm_table[i].attached_count;
            return SHM_OK;
        }
    }
    
    return SHM_NOT_FOUND;
}
