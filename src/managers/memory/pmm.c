#include "pmm.h"

// External functions
extern void vga_print(const char *s);
static void print_hex(uint32_t val) {
    char hex[9];
    hex[8] = '\0';
    for (int i = 7; i >= 0; i--) {
        uint32_t digit = val & 0xF;
        hex[i] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
        val >>= 4;
    }
    vga_print(hex);
}

// Bitmap and memory tracking
static uint32_t *bitmap = 0;
static uint32_t total_pages = 0;
static uint32_t used_pages = 0;
static uint32_t bitmap_size = 0;
static uint32_t memory_start = 0;

// Helper function to convert address to page index
static uint32_t addr_to_page(uint32_t addr) {
    return (addr - memory_start) / PAGE_SIZE;
}

// Helper function to convert page index to address
static uint32_t page_to_addr(uint32_t page) {
    return memory_start + (page * PAGE_SIZE);
}

// Set a bit in the bitmap (mark page as used)
static void bitmap_set(uint32_t page) {
    uint32_t byte = page / 32;
    uint32_t bit = page % 32;
    bitmap[byte] |= (1 << bit);
}

// Clear a bit in the bitmap (mark page as free)
static void bitmap_clear(uint32_t page) {
    uint32_t byte = page / 32;
    uint32_t bit = page % 32;
    bitmap[byte] &= ~(1 << bit);
}

// Test if a bit is set in the bitmap
static int bitmap_test(uint32_t page) {
    uint32_t byte = page / 32;
    uint32_t bit = page % 32;
    return bitmap[byte] & (1 << bit);
}

// Find the end of kernel (assuming kernel starts at 1MB)
extern uint32_t kernel_end;  // Will be defined in linker script

int pmm_init(multiboot_info_t *mbi) {
    // Calculate total memory (upper memory in KB, convert to bytes)
    uint32_t total_memory = (mbi->mem_upper * 1024) + 0x100000;  // Add 1MB
    
    // Memory starts at 1MB (0x00100000)
    memory_start = 0x00100000;
    
    // Calculate total pages
    total_pages = (total_memory - memory_start) / PAGE_SIZE;
    
    // Calculate bitmap size (1 bit per page, so divide by 8 to get bytes)
    // Then divide by 4 to get uint32_t count
    bitmap_size = (total_pages + 31) / 32;  // Round up
    
    // Find where to place bitmap (after kernel and all modules)
    uint32_t bitmap_addr = (uint32_t)&kernel_end;
    
    // Check if modules are loaded and find highest address
    if (mbi->flags & 0x8) {
        multiboot_module_t *mod = (multiboot_module_t *)mbi->mods_addr;
        for (uint32_t i = 0; i < mbi->mods_count; i++) {
            if (mod[i].mod_end > bitmap_addr) {
                bitmap_addr = mod[i].mod_end;
            }
        }
    }
    
    // Align bitmap to page boundary
    bitmap_addr = (bitmap_addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    
    // Set bitmap pointer
    bitmap = (uint32_t *)bitmap_addr;
    
    // Initialize bitmap - mark all pages as free (0)
    for (uint32_t i = 0; i < bitmap_size; i++) {
        bitmap[i] = 0;
    }
    
    // Mark all pages as free initially
    used_pages = 0;
    
    // Mark used regions
    // 1. Mark kernel region (1MB to kernel_end)
    pmm_mark_region_used(0x00100000, (uint32_t)&kernel_end);
    
    // 2. Mark modules as used
    if (mbi->flags & 0x8) {
        multiboot_module_t *mod = (multiboot_module_t *)mbi->mods_addr;
        for (uint32_t i = 0; i < mbi->mods_count; i++) {
            pmm_mark_region_used(mod[i].mod_start, mod[i].mod_end);
        }
    }
    
    // 3. Mark bitmap itself as used
    uint32_t bitmap_end = bitmap_addr + (bitmap_size * 4);
    pmm_mark_region_used(bitmap_addr, bitmap_end);
    
    return 1;  /* Success */
}

void pmm_mark_region_used(uint32_t start, uint32_t end) {
    // Align to page boundaries
    uint32_t start_page = addr_to_page(start & ~(PAGE_SIZE - 1));
    uint32_t end_page = addr_to_page((end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));
    
    for (uint32_t page = start_page; page < end_page && page < total_pages; page++) {
        if (!bitmap_test(page)) {
            bitmap_set(page);
            used_pages++;
        }
    }
}

void *pmm_alloc_page() {
    // Find first free page
    for (uint32_t page = 0; page < total_pages; page++) {
        if (!bitmap_test(page)) {
            bitmap_set(page);
            used_pages++;
            uint32_t addr = page_to_addr(page);
            return (void *)addr;
        }
    }
    
    // No free pages!
    return 0;
}

void pmm_free_page(void *addr) {
    uint32_t page = addr_to_page((uint32_t)addr);
    
    if (page >= total_pages) {
        return;  // Invalid address
    }
    
    if (bitmap_test(page)) {
        bitmap_clear(page);
        used_pages--;
    }
}

void pmm_print_stats() {
    vga_print("PMM Stats: ");
    print_hex(total_pages - used_pages);
    vga_print(" pages free / ");
    print_hex(total_pages);
    vga_print(" total (");
    print_hex((total_pages - used_pages) * PAGE_SIZE / (1024 * 1024));
    vga_print(" MB free)\n");
}

uint32_t pmm_get_free_pages() {
    return total_pages - used_pages;
}

uint32_t pmm_get_total_pages() {
    return total_pages;
}
