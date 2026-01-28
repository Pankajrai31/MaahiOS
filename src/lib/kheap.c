/*
 * DYNAMIC Kernel Heap - Bump allocator
 * Allocates from identity-mapped but unreserved space (after kernel structures)
 * Uses the gap between actual usage and 128MB identity map limit
 */

#include <stdint.h>
#include "kheap.h"

/* External serial functions */
extern void serial_print(const char *s);
extern void serial_hex(uint8_t byte);

/* Heap boundaries (set during init) */
static unsigned int heap_start = 0;
static unsigned int heap_end = 0;
static unsigned int heap_current = 0;

/* External PMM functions */
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
    
    /* Log heap initialization */
    serial_print("[KHEAP] Initialized: start=");
    serial_hex((heap_start >> 24) & 0xFF);
    serial_hex((heap_start >> 16) & 0xFF);
    serial_hex((heap_start >> 8) & 0xFF);
    serial_hex(heap_start & 0xFF);
    serial_print(" end=");
    serial_hex((heap_end >> 24) & 0xFF);
    serial_hex((heap_end >> 16) & 0xFF);
    serial_hex((heap_end >> 8) & 0xFF);
    serial_hex(heap_end & 0xFF);
    serial_print(" size=");
    unsigned int size_mb = (heap_end - heap_start) / (1024*1024);
    serial_hex((size_mb >> 8) & 0xFF);
    serial_hex(size_mb & 0xFF);
    serial_print("MB\n");
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
        serial_print("[KMALLOC] OUT OF MEMORY! Requested=");
        serial_hex((size >> 24) & 0xFF);
        serial_hex((size >> 16) & 0xFF);
        serial_hex((size >> 8) & 0xFF);
        serial_hex(size & 0xFF);
        serial_print(" current=");
        serial_hex((heap_current >> 24) & 0xFF);
        serial_hex((heap_current >> 16) & 0xFF);
        serial_hex((heap_current >> 8) & 0xFF);
        serial_hex(heap_current & 0xFF);
        serial_print(" end=");
        serial_hex((heap_end >> 24) & 0xFF);
        serial_hex((heap_end >> 16) & 0xFF);
        serial_hex((heap_end >> 8) & 0xFF);
        serial_hex(heap_end & 0xFF);
        serial_print("\n");
        return 0;  /* Out of memory */
    }
    
    /* Warning when heap usage > 90% */
    unsigned int used = heap_current - heap_start;
    unsigned int total = heap_end - heap_start;
    if (used * 100 > total * 90) {
        static int warned = 0;
        if (!warned) {
            serial_print("[KMALLOC] WARNING: Heap usage > 90%\n");
            warned = 1;
        }
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
        
        /* CRITICAL: Flush TLB after extending heap with new pages
         * The identity-mapped pages were mapped during paging_init(),
         * but PMM allocations change physical page ownership.
         * Without TLB flush, CPU uses stale TLB entries → page faults/freezes
         * This is especially critical during frequent window operations
         * which trigger rapid kmalloc() calls for PCB allocation. */
        asm volatile("mov %%cr3, %%eax; mov %%eax, %%cr3" ::: "eax", "memory");
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
        
        /* Flush TLB after heap extension (same reason as kmalloc) */
        asm volatile("mov %%cr3, %%eax; mov %%eax, %%cr3" ::: "eax", "memory");
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
