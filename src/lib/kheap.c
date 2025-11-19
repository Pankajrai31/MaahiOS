/*
 * DYNAMIC Kernel Heap - Bump allocator
 * Allocates from identity-mapped but unreserved space (after kernel structures)
 * Uses the gap between actual usage and 128MB identity map limit
 */

#include "kheap.h"

/* Heap boundaries (set during init) */
static unsigned int heap_start = 0;
static unsigned int heap_end = 0;
static unsigned int heap_current = 0;

/* External VGA and PMM for debug */
extern void vga_puts(const char *str);
extern void vga_put_hex(unsigned int val);
extern void* pmm_alloc_page(void);

/* Initialize kernel heap - use identity-mapped space after kernel structures */
void kheap_init(void) {
    /* Get first free page from PMM (this is right after kernel+sysman+bitmap+structures) */
    /* PMM has reserved actual usage, so this gives us the start of free space */
    heap_start = (unsigned int)pmm_alloc_page();
    
    /* Heap ends at 128MB (identity map limit) */
    heap_end = 0x08000000;  /* 128MB */
    
    /* Start allocating from heap_start */
    heap_current = heap_start;
}

/* Bump allocator - allocates from identity-mapped free space */
void* kmalloc(size_t size) {
    if (size == 0) {
        return 0;
    }
    
    /* Align to 16 bytes */
    size = (size + 15) & ~15;
    
    /* Check if we have space */
    if (heap_current + size > heap_end) {
        return 0;  /* Out of memory */
    }
    
    /* Allocate */
    void *ptr = (void *)heap_current;
    heap_current += size;
    
    /* Allocate physical pages as needed (they're identity-mapped already) */
    /* Round up to page boundary and mark as used in PMM */
    unsigned int page_end = (heap_current + 4095) & ~4095;
    static unsigned int last_page_allocated = 0;
    
    if (page_end > last_page_allocated) {
        /* Allocate pages from PMM to cover this allocation */
        while (last_page_allocated < page_end) {
            pmm_alloc_page();  /* This marks the page as used */
            last_page_allocated += 4096;
        }
    }
    
    return ptr;
}

/* Aligned allocation for DMA buffers, etc. */
void* kmalloc_aligned(size_t size, size_t alignment) {
    if (size == 0) {
        return 0;
    }
    
    /* Align heap_current to the requested alignment */
    unsigned int aligned_current = (heap_current + alignment - 1) & ~(alignment - 1);
    
    /* Check if we have space */
    if (aligned_current + size > heap_end) {
        return 0;  /* Out of memory */
    }
    
    /* Allocate */
    heap_current = aligned_current;
    void *ptr = (void *)heap_current;
    heap_current += size;
    
    /* Allocate physical pages as needed */
    unsigned int page_end = (heap_current + 4095) & ~4095;
    static unsigned int last_page_allocated_aligned = 0;
    
    if (page_end > last_page_allocated_aligned) {
        while (last_page_allocated_aligned < page_end) {
            pmm_alloc_page();
            last_page_allocated_aligned += 4096;
        }
    }
    
    return ptr;
}

/* Dummy free - does nothing for now */
void kfree(void* ptr) {
    /* Bump allocator doesn't free - ignore */
    (void)ptr;
}

/* Get heap stats */
void kheap_stats(unsigned int *total_pages, unsigned int *used_bytes, unsigned int *free_bytes) {
    *total_pages = (heap_end - heap_start) / 4096;
    *used_bytes = heap_current - heap_start;
    *free_bytes = heap_end - heap_current;
}
