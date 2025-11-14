/*
 * Kernel Heap Allocator for MaahiOS
 * 
 * Uses a free-list algorithm with first-fit allocation.
 * Each block has a header tracking size and free/used status.
 * Properly handles virtual memory by mapping physical pages.
 */

#include "kheap.h"
#include "../managers/memory/paging.h"
#include "../managers/memory/pmm.h"

/* Forward declare VGA functions for debug output */
extern void vga_puts(const char *str);
extern void vga_put_hex(unsigned int val);

/* Kernel heap virtual address space (above identity map) */
#define KHEAP_VIRTUAL_START 0xC0400000  /* Start at 3GB + 4MB */
#define KHEAP_VIRTUAL_END   0xD0000000  /* End at 3.25GB (192MB heap) */
static unsigned int kheap_next_virtual = KHEAP_VIRTUAL_START;

/* Block header structure */
typedef struct kheap_block_header {
    size_t size;                            /* Size of usable data (excluding header) */
    int is_free;                            /* 1 = free, 0 = used */
    struct kheap_block_header *next;        /* Next block in list */
    unsigned int magic;                     /* Magic number for corruption detection */
} kheap_block_header_t;

#define BLOCK_HEADER_SIZE sizeof(kheap_block_header_t)
#define PAGE_SIZE 4096
#define ALIGN_SIZE 8  /* Align allocations to 8 bytes */
#define HEAP_MAGIC 0xDEADBEEF  /* Magic number for validation */

/* Head of free list */
static kheap_block_header_t *kheap_start = 0;

/* Statistics */
static unsigned int total_pages_allocated = 0;

/* Helper: Align size to ALIGN_SIZE boundary */
static size_t align_size(size_t size) {
    return (size + ALIGN_SIZE - 1) & ~(ALIGN_SIZE - 1);
}

/* Request a new page, map it, and add to heap */
static void expand_kheap(void) {
    /* Check if we have virtual address space left */
    if (kheap_next_virtual + PAGE_SIZE > KHEAP_VIRTUAL_END) {
        vga_puts("[KHEAP] ERROR: Kernel heap virtual space exhausted!\n");
        return;
    }
    
    /* 1. Allocate physical page from PMM */
    void *phys_page = pmm_alloc_page();
    if (!phys_page) {
        vga_puts("[KHEAP] ERROR: PMM allocation failed!\n");
        return;
    }
    
    /* 2. Get next virtual address for this page */
    unsigned int virt_addr = kheap_next_virtual;
    kheap_next_virtual += PAGE_SIZE;
    
    /* 3. Map physical page to virtual address in kernel page directory */
    paging_map_page(kernel_page_directory, virt_addr, (unsigned int)phys_page, 
                    PAGE_PRESENT | PAGE_WRITE);
    
    total_pages_allocated++;
    
    /* 4. Now we can safely access the page via virtual address */
    kheap_block_header_t *block = (kheap_block_header_t *)virt_addr;
    block->size = PAGE_SIZE - BLOCK_HEADER_SIZE;
    block->is_free = 1;
    block->next = 0;
    block->magic = HEAP_MAGIC;
    
    /* Add to free list */
    if (!kheap_start) {
        kheap_start = block;
    } else {
        /* Add to end of list */
        kheap_block_header_t *current = kheap_start;
        while (current->next) {
            current = current->next;
        }
        current->next = block;
    }
}

/* Initialize kernel heap */
void kheap_init(void) {
    kheap_start = 0;
    total_pages_allocated = 0;
    vga_puts("Kernel heap initialized\n");
}

/* Allocate memory from kernel heap */
void* kmalloc(size_t size) {
    if (size == 0) {
        return 0;
    }
    
    /* Align size */
    size = align_size(size);
    
    /* Search for free block with first-fit */
    kheap_block_header_t *current = kheap_start;
    
    while (current) {
        /* Verify magic number */
        if (current->magic != HEAP_MAGIC) {
            vga_puts("[KHEAP] CORRUPTION DETECTED at 0x");
            vga_put_hex((unsigned int)current);
            vga_puts("\n");
            return 0;
        }
        
        if (current->is_free && current->size >= size) {
            /* Found suitable block */
            current->is_free = 0;
            
            /* Split block if remaining space is large enough */
            if (current->size >= size + BLOCK_HEADER_SIZE + ALIGN_SIZE) {
                /* Create new free block from remaining space */
                kheap_block_header_t *new_block = (kheap_block_header_t *)((char *)current + BLOCK_HEADER_SIZE + size);
                new_block->size = current->size - size - BLOCK_HEADER_SIZE;
                new_block->is_free = 1;
                new_block->next = current->next;
                new_block->magic = HEAP_MAGIC;
                
                current->size = size;
                current->next = new_block;
            }
            
            /* Return pointer to data (after header) */
            return (void *)((char *)current + BLOCK_HEADER_SIZE);
        }
        current = current->next;
    }
    
    /* No suitable block found - need to expand heap */
    expand_kheap();
    
    /* Try again after expansion */
    if (kheap_start) {
        return kmalloc(size);
    }
    
    return 0;
}

/* Free memory back to kernel heap */
void kfree(void* ptr) {
    if (!ptr) {
        return;
    }
    
    /* Get block header */
    kheap_block_header_t *block = (kheap_block_header_t *)((char *)ptr - BLOCK_HEADER_SIZE);
    
    /* Verify magic number */
    if (block->magic != HEAP_MAGIC) {
        vga_puts("[KHEAP] CORRUPTION: Invalid magic in kfree at 0x");
        vga_put_hex((unsigned int)ptr);
        vga_puts("\n");
        return;
    }
    
    block->is_free = 1;
    
    /* Coalesce with next block if it's free */
    if (block->next && block->next->is_free) {
        block->size += BLOCK_HEADER_SIZE + block->next->size;
        block->next = block->next->next;
    }
    
    /* TODO: Coalesce with previous block (requires doubly-linked list) */
}

/* Reallocate memory (resize) */
void* krealloc(void* ptr, size_t new_size) {
    if (!ptr) {
        return kmalloc(new_size);
    }
    
    if (new_size == 0) {
        kfree(ptr);
        return 0;
    }
    
    /* Get current block */
    kheap_block_header_t *block = (kheap_block_header_t *)((char *)ptr - BLOCK_HEADER_SIZE);
    
    /* If new size fits in current block, just return same pointer */
    if (align_size(new_size) <= block->size) {
        return ptr;
    }
    
    /* Allocate new block */
    void *new_ptr = kmalloc(new_size);
    if (!new_ptr) {
        return 0;
    }
    
    /* Copy old data to new block */
    char *src = (char *)ptr;
    char *dst = (char *)new_ptr;
    for (size_t i = 0; i < block->size && i < new_size; i++) {
        dst[i] = src[i];
    }
    
    /* Free old block */
    kfree(ptr);
    
    return new_ptr;
}

/* Allocate and zero memory */
void* kcalloc(size_t count, size_t size) {
    size_t total_size = count * size;
    void *ptr = kmalloc(total_size);
    
    if (ptr) {
        /* Zero the memory */
        char *p = (char *)ptr;
        for (size_t i = 0; i < total_size; i++) {
            p[i] = 0;
        }
    }
    
    return ptr;
}

/* Get heap statistics */
void kheap_stats(unsigned int *total_pages, unsigned int *used_bytes, unsigned int *free_bytes) {
    *total_pages = total_pages_allocated;
    
    unsigned int used = 0;
    unsigned int free_space = 0;
    
    kheap_block_header_t *current = kheap_start;
    while (current) {
        if (current->is_free) {
            free_space += current->size;
        } else {
            used += current->size;
        }
        current = current->next;
    }
    
    *used_bytes = used;
    *free_bytes = free_space;
}
