/**
 * MaahiOS Paging Manager
 * 
 * Virtual memory management using x86 two-level page tables.
 * - Identity maps 128MB for kernel space
 * - Supports dynamic page mapping for processes and MMIO
 * - VMM wrappers for future full virtual memory manager
 */

#include "paging.h"
#include "pmm.h"
#include "../klog/klog.h"

/* Global page directory pointer (exposed for kheap) */
uint32_t *kernel_page_directory = 0;
static uint32_t identity_map_end = 0;

// Map a single 4KB page
void paging_map_page(uint32_t *page_dir, uint32_t virt, uint32_t phys, uint32_t flags) {
    uint32_t page_dir_idx = virt >> 22;  // Top 10 bits
    uint32_t page_table_idx = (virt >> 12) & 0x3FF;  // Middle 10 bits
    
    // Get or create page table
    uint32_t *page_table;
    if (!(page_dir[page_dir_idx] & PAGE_PRESENT)) {
        // Allocate new page table
        page_table = (uint32_t *)pmm_alloc_page();
        
        // Sanity check - page table should be in high memory
        if ((uint32_t)page_table < 0x02400000) {
            KLOG_ERROR_HEX("PAGING", "Page table allocated in reserved region", (uint32_t)page_table);
        }
        
        // Clear page table
        for (int i = 0; i < ENTRIES_PER_TABLE; i++) {
            page_table[i] = 0;
        }
        
        // Install page table in directory
        page_dir[page_dir_idx] = ((uint32_t)page_table) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    } else {
        // Page table already exists
        page_table = (uint32_t *)(page_dir[page_dir_idx] & 0xFFFFF000);
    }
    
    // Map page in page table
    page_table[page_table_idx] = (phys & 0xFFFFF000) | flags;
    
    // Flush TLB for this specific page (industry-standard practice)
    // CRITICAL: TLB must be invalidated after modifying page tables
    // Without this, CPU uses stale TLB entries → page faults/freezes
    asm volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

// Identity map a region of memory
void identity_map_region(uint32_t *page_dir, uint32_t start, uint32_t end) {
    // Align to page boundaries
    start = start & 0xFFFFF000;
    end = (end + PAGE_SIZE_4KB - 1) & 0xFFFFF000;
    
    // Map each page (with USER flag for Ring 3 access)
    for (uint32_t addr = start; addr < end; addr += PAGE_SIZE_4KB) {
        paging_map_page(page_dir, addr, addr, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    }
}

// Enable paging by setting CR0 and CR3
void paging_enable() {
    if (!kernel_page_directory) {
        KLOG_ERROR("PAGING", "Cannot enable paging - no page directory!");
        return;
    }
    
    // Load page directory into CR3
    asm volatile("mov %0, %%cr3" : : "r"(kernel_page_directory));
    
    // Enable paging in CR0 (also enable write protect for Ring 0)
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80010001;  // Set PG (bit 31), WP (bit 16), PE (bit 0)
    asm volatile("mov %0, %%cr0" : : "r"(cr0));
    
    // Flush TLB by reloading CR3
    asm volatile("mov %%cr3, %%eax; mov %%eax, %%cr3" ::: "eax");
}

// Find highest address used by GRUB (kernel + modules + bitmap)
static uint32_t find_highest_used_address(multiboot_info_t *mbi) {
    uint32_t highest = 0x00200000;  // Start at 2MB minimum
    
    // Check kernel end
    extern uint32_t kernel_end;
    if ((uint32_t)&kernel_end > highest) {
        highest = (uint32_t)&kernel_end;
    }
    
    // Check modules
    if (mbi->flags & 0x8) {
        multiboot_module_t *mod = (multiboot_module_t *)mbi->mods_addr;
        for (uint32_t i = 0; i < mbi->mods_count; i++) {
            if (mod[i].mod_end > highest) {
                highest = mod[i].mod_end;
            }
        }
    }
    
    // Add 1MB buffer for PMM bitmap and other kernel data
    highest += 0x00100000;
    
    // Round up to 4MB boundary for clean mapping
    highest = (highest + 0x003FFFFF) & 0xFFC00000;
    
    // Ensure at least 32MB
    if (highest < IDENTITY_MAP_SIZE) {
        highest = IDENTITY_MAP_SIZE;
    }
    
    return highest;
}

// Initialize paging with identity mapping
int paging_init(multiboot_info_t *mbi) {
    // Calculate where kernel and modules end
    uint32_t kernel_modules_end = find_highest_used_address(mbi);
    
    // Calculate reservation point: kernel + modules + 512KB buffer
    uint32_t reservation_end = kernel_modules_end + 0x00080000;  // +512KB
    
    // Round up to 4MB boundary for clean mapping
    reservation_end = (reservation_end + 0x003FFFFF) & 0xFFC00000;
    
    // NOTE: PMM already marked kernel+modules+bitmap as used, so we don't need to mark again
    
    // Allocate page directory - PMM will give us first free page after bitmap
    kernel_page_directory = (uint32_t *)pmm_alloc_page();
    
    // Clear page directory
    for (int i = 0; i < ENTRIES_PER_TABLE; i++) {
        kernel_page_directory[i] = 0;
    }
    
    // Calculate actual used memory (kernel + modules + bitmap + structures)
    identity_map_end = reservation_end;
    
    // Add space for page directory and page tables (1MB should be enough)
    if ((uint32_t)kernel_page_directory + 0x00100000 > identity_map_end) {
        identity_map_end = (uint32_t)kernel_page_directory + 0x00100000;
    }
    
    // Round up to 4MB boundary
    uint32_t actual_used = (identity_map_end + 0x003FFFFF) & 0xFFC00000;
    
    // CRITICAL: Reserve ONLY the actually used region in PMM
    pmm_mark_region_used(0x00100000, actual_used);
    
    // BUT: Identity map 128MB for kernel space (leaves room for growth)
    // This space is NOT reserved by PMM - available for kmalloc() on demand
    identity_map_end = 0x08000000;  // 128MB
    
    /* Identity mapped 128MB */
    
    // Identity map the entire 128MB region (but PMM only reserves actual usage)
    identity_map_region(kernel_page_directory, 0x00000000, identity_map_end);
    
    // Note: Graphics framebuffer will be mapped manually by kernel after paging init
    
    // Enable paging
    paging_enable();
    
    KLOG_INFO("PAGING", "Initialized: identity mapped 128MB, page dir at 0x%x",
              (uint32_t)kernel_page_directory);
    
    return 1;  /* Success */
}

/* ===========================================================================
 * Per-Process Page Directory Functions
 * =========================================================================== */

/**
 * Get the kernel page directory pointer.
 */
uint32_t *paging_get_kernel_directory(void) {
    return kernel_page_directory;
}

/**
 * Clone the kernel page directory for a new process.
 * Copies all kernel-space page directory entries (identity-mapped regions,
 * MMIO, SHM mappings at 0x80000000+) so the new process can access
 * kernel services, SHM, and shared memory.
 * User-space entries (below identity_map_end) that belong to other processes
 * are NOT copied — the new process gets a clean user address space.
 */
uint32_t *paging_clone_kernel_directory(void) {
    /* Allocate a new page directory (4KB aligned from PMM) */
    uint32_t *new_dir = (uint32_t *)pmm_alloc_page();
    if (!new_dir) {
        KLOG_ERROR("PAGING", "Failed to allocate page directory");
        return 0;
    }
    
    /* Copy ALL entries from kernel page directory.
     * This gives the new process:
     * - Identity-mapped kernel space (0x00000000 - 0x08000000)
     * - MMIO regions (framebuffer, PCI BARs)
     * - SHM mappings (0x80000000+)
     * The new process will add its own user mappings separately.
     */
    for (int i = 0; i < ENTRIES_PER_TABLE; i++) {
        new_dir[i] = kernel_page_directory[i];
    }
    
    KLOG_DEBUG_HEX("PAGING", "Cloned kernel page directory at", (uint32_t)new_dir);
    return new_dir;
}

/**
 * Map a contiguous physical region into a process page directory.
 * Used to load .mex app code/data at the app's virtual base address.
 *
 * @param page_dir  Target page directory
 * @param virt_start Virtual address to map at (must be page-aligned)
 * @param phys_start Physical address of the memory (must be page-aligned)
 * @param size      Size in bytes (rounded up to pages)
 * @param flags     Page flags (PAGE_PRESENT | PAGE_WRITE | PAGE_USER)
 */
void paging_map_user_region(uint32_t *page_dir, uint32_t virt_start, uint32_t phys_start, uint32_t size, uint32_t flags) {
    /* Align to page boundaries */
    virt_start = virt_start & 0xFFFFF000;
    phys_start = phys_start & 0xFFFFF000;
    uint32_t pages = (size + PAGE_SIZE_4KB - 1) / PAGE_SIZE_4KB;
    
    for (uint32_t i = 0; i < pages; i++) {
        uint32_t virt = virt_start + (i * PAGE_SIZE_4KB);
        uint32_t phys = phys_start + (i * PAGE_SIZE_4KB);
        paging_map_page(page_dir, virt, phys, flags);
    }
    
    KLOG_DEBUG_HEX2("PAGING", "Mapped user region virt/pages: ", virt_start, pages);
}

/**
 * Unmap a virtual region from a process page directory.
 * Clears page table entries but does NOT free physical memory.
 * (Physical memory is freed separately by the caller.)
 */
void paging_unmap_user_region(uint32_t *page_dir, uint32_t virt_start, uint32_t size) {
    virt_start = virt_start & 0xFFFFF000;
    uint32_t pages = (size + PAGE_SIZE_4KB - 1) / PAGE_SIZE_4KB;
    
    for (uint32_t i = 0; i < pages; i++) {
        uint32_t virt = virt_start + (i * PAGE_SIZE_4KB);
        uint32_t pd_idx = virt >> 22;
        uint32_t pt_idx = (virt >> 12) & 0x3FF;
        
        if (page_dir[pd_idx] & PAGE_PRESENT) {
            uint32_t *page_table = (uint32_t *)(page_dir[pd_idx] & 0xFFFFF000);
            page_table[pt_idx] = 0;
            asm volatile("invlpg (%0)" :: "r"(virt) : "memory");
        }
    }
}

/**
 * Destroy a per-process page directory.
 * Frees page tables that were allocated specifically for user-space mappings.
 * Does NOT free page tables shared with the kernel directory.
 */
void paging_destroy_directory(uint32_t *page_dir) {
    if (!page_dir || page_dir == kernel_page_directory) {
        return;  /* Never destroy the kernel's own directory */
    }
    
    /* Free page tables that were created for this process
     * (i.e., page tables that differ from the kernel directory).
     * Kernel-shared page tables must NOT be freed. */
    for (int i = 0; i < ENTRIES_PER_TABLE; i++) {
        if ((page_dir[i] & PAGE_PRESENT) && 
            page_dir[i] != kernel_page_directory[i]) {
            /* This page table was allocated specifically for this process */
            uint32_t *pt = (uint32_t *)(page_dir[i] & 0xFFFFF000);
            pmm_free_page(pt);
        }
    }
    
    /* Free the page directory itself */
    pmm_free_page(page_dir);
    KLOG_DEBUG_HEX("PAGING", "Destroyed page directory at", (uint32_t)page_dir);
}

/**
 * Switch the active page directory (load CR3).
 * Called during context switch when switching to a process
 * with a different page directory.
 */
void paging_switch_directory(uint32_t *page_dir) {
    asm volatile("mov %0, %%cr3" : : "r"(page_dir) : "memory");
}

// VMM wrapper functions (simple for now, full VMM in Phase 3)

void *vmm_alloc_page() {
    // For now, just call PMM directly
    // PMM will return pages outside identity region (thanks to reservation)
    void *page = pmm_alloc_page();
    
    // In Phase 3, this will:
    // 1. Allocate physical page from PMM
    // 2. Find free virtual address for current process
    // 3. Map virtual -> physical in process page table
    // 4. Return virtual address
    
    return page;
}

void *vmm_alloc_size(uint32_t size_bytes) {
    extern uint32_t *kernel_page_directory;
    
    // 1. Allocate physical pages from PMM
    void *mem = pmm_alloc_size(size_bytes);
    
    if (!mem) {
        return 0;
    }
    
    // 2. Identity map the allocated region so it's accessible from ring 3
    uint32_t start_addr = (uint32_t)mem;
    uint32_t end_addr = start_addr + size_bytes;
    
    identity_map_region(kernel_page_directory, start_addr, end_addr);
    
    // 3. Return the address (now mapped and accessible)
    return mem;
}

void vmm_free_page(void *addr) {
    // For now, just call PMM directly
    pmm_free_page(addr);
    
    // In Phase 3, this will:
    // 1. Look up virtual -> physical mapping
    // 2. Unmap from page table
    // 3. Free physical page via PMM
}

/**
 * Map a MMIO (Memory-Mapped I/O) region into kernel address space
 * Used for PCI device BARs like AHCI controller registers
 */
void paging_map_mmio_region(uint32_t phys_start, uint32_t size) {
    if (!kernel_page_directory) {
        KLOG_ERROR("PAGING", "Cannot map MMIO - paging not initialized");
        return;
    }
    
    // Align to page boundaries
    uint32_t phys_aligned = phys_start & 0xFFFFF000;
    uint32_t size_aligned = (size + PAGE_SIZE_4KB - 1) & 0xFFFFF000;
    
    /* Mapping MMIO region */
    
    // Identity map the MMIO region (virtual = physical for simplicity)
    for (uint32_t offset = 0; offset < size_aligned; offset += PAGE_SIZE_4KB) {
        uint32_t addr = phys_aligned + offset;
        paging_map_page(kernel_page_directory, addr, addr, 
                       PAGE_PRESENT | PAGE_WRITE);  // No USER flag for MMIO
    }
    
    // Flush TLB to ensure mappings take effect
    asm volatile("mov %%cr3, %%eax; mov %%eax, %%cr3" ::: "eax");
    
    /* MMIO region mapped */
}
