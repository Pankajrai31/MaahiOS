#include "paging.h"
#include "pmm.h"

// External VGA functions
extern void vga_puts(const char *str);
extern void vga_put_hex(uint32_t num);

// Global page directory pointer (exposed for kheap)
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
            vga_puts("ERROR: Page table allocated in reserved region: 0x");
            vga_put_hex((uint32_t)page_table);
            vga_puts("\n");
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
        vga_puts("ERROR: Cannot enable paging - no page directory!\n");
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
    
    vga_puts("[PAGING] Identity mapping 0x00000000 - 0x08000000 (128MB)\n");
    vga_puts("[PAGING] PMM reserved: 0x00100000 - 0x");
    vga_put_hex(actual_used);
    vga_puts("\n");
    
    // Identity map the entire 128MB region (but PMM only reserves actual usage)
    identity_map_region(kernel_page_directory, 0x00000000, identity_map_end);
    
    // Enable paging
    paging_enable();
    
    return 1;  /* Success */
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

void vmm_free_page(void *addr) {
    // For now, just call PMM directly
    pmm_free_page(addr);
    
    // In Phase 3, this will:
    // 1. Look up virtual -> physical mapping
    // 2. Unmap from page table
    // 3. Free physical page via PMM
}
