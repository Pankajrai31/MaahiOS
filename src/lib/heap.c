/*
 * Simple Heap Allocator for MaahiOS
 * 
 * Uses a free-list algorithm with first-fit allocation.
 * Each block has a header tracking size and free/used status.
 */

#include "heap.h"
#include "../syscalls/user_syscalls.h"

/* Block header structure */
typedef struct block_header {
    size_t size;                    /* Size of usable data (excluding header) */
    int is_free;                    /* 1 = free, 0 = used */
    struct block_header *next;      /* Next block in list */
} block_header_t;

#define BLOCK_HEADER_SIZE sizeof(block_header_t)
#define PAGE_SIZE 4096
#define ALIGN_SIZE 8  /* Align allocations to 8 bytes */

/* Head of free list */
static block_header_t *heap_start = 0;

/* Statistics */
static unsigned int total_pages_allocated = 0;
static unsigned int syscalls_made = 0;

/* Debug output function */
extern void syscall_puts(const char *str);
extern void syscall_putchar(char c);

/* Helper: Align size to ALIGN_SIZE boundary */
static size_t align_size(size_t size) {
    return (size + ALIGN_SIZE - 1) & ~(ALIGN_SIZE - 1);
}

/* Request a new page from kernel and add to heap */
static void expand_heap(void) {
    syscall_puts("[HEAP] Requesting new page via syscall...\n");
    
    /* Request page from kernel */
    void *new_page = syscall_alloc_page();
    syscalls_made++;
    total_pages_allocated++;
    
    if (!new_page) {
        syscall_puts("[HEAP] ERROR: Failed to allocate page!\n");
        return;
    }
    
    /* Create block header at start of page */
    block_header_t *block = (block_header_t *)new_page;
    block->size = PAGE_SIZE - BLOCK_HEADER_SIZE;
    block->is_free = 1;
    block->next = 0;
    
    /* Add to free list */
    if (!heap_start) {
        heap_start = block;
    } else {
        /* Add to end of list */
        block_header_t *current = heap_start;
        while (current->next) {
            current = current->next;
        }
        current->next = block;
    }
    
    syscall_puts("[HEAP] Page added to heap: 0x");
    /* Print address in hex */
    unsigned int addr = (unsigned int)new_page;
    for (int i = 28; i >= 0; i -= 4) {
        int digit = (addr >> i) & 0xF;
        syscall_putchar(digit < 10 ? '0' + digit : 'A' + digit - 10);
    }
    syscall_puts("\n");
}

/* Initialize heap */
void heap_init(void) {
    heap_start = 0;
    total_pages_allocated = 0;
    syscalls_made = 0;
    syscall_puts("[HEAP] Heap allocator initialized\n");
}

/* Allocate memory */
void* malloc(size_t size) {
    if (size == 0) {
        return 0;
    }
    
    /* Align size */
    size = align_size(size);
    
    /* Search for free block with first-fit */
    block_header_t *current = heap_start;
    
    while (current) {
        if (current->is_free && current->size >= size) {
            /* Found suitable block */
            current->is_free = 0;
            
            /* Split block if remaining space is large enough */
            if (current->size >= size + BLOCK_HEADER_SIZE + ALIGN_SIZE) {
                /* Create new free block from remaining space */
                block_header_t *new_block = (block_header_t *)((char *)current + BLOCK_HEADER_SIZE + size);
                new_block->size = current->size - size - BLOCK_HEADER_SIZE;
                new_block->is_free = 1;
                new_block->next = current->next;
                
                current->size = size;
                current->next = new_block;
            }
            
            /* Return pointer to data (after header) */
            return (void *)((char *)current + BLOCK_HEADER_SIZE);
        }
        current = current->next;
    }
    
    /* No suitable block found - need to expand heap */
    expand_heap();
    
    /* Try again after expansion */
    if (heap_start) {
        return malloc(size);
    }
    
    return 0;
}

/* Free memory */
void free(void* ptr) {
    if (!ptr) {
        return;
    }
    
    /* Get block header */
    block_header_t *block = (block_header_t *)((char *)ptr - BLOCK_HEADER_SIZE);
    block->is_free = 1;
    
    /* Coalesce with next block if it's free */
    if (block->next && block->next->is_free) {
        block->size += BLOCK_HEADER_SIZE + block->next->size;
        block->next = block->next->next;
    }
    
    /* TODO: Coalesce with previous block (requires doubly-linked list) */
}

/* Get heap statistics */
void heap_stats(unsigned int *total_pages, unsigned int *used_bytes, unsigned int *free_bytes) {
    *total_pages = total_pages_allocated;
    
    unsigned int used = 0;
    unsigned int free_space = 0;
    
    block_header_t *current = heap_start;
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
