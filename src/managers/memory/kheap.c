/**
 * MaahiOS Kernel Heap
 * Simple heap allocator for kernel use
 */

#include <stdint.h>
#include <stddef.h>

/* Heap configuration */
#define KHEAP_START     0x01000000   /* 16MB mark */
#define KHEAP_SIZE      0x00400000   /* 4MB heap */
#define KHEAP_END       (KHEAP_START + KHEAP_SIZE)

/* Block header */
typedef struct block_header {
    uint32_t size;          /* Size of this block (including header) */
    uint32_t is_free;       /* 1 if free, 0 if allocated */
    struct block_header *next;
    struct block_header *prev;
} block_header_t;

/* Heap state */
static block_header_t *heap_start = NULL;
static block_header_t *heap_end = NULL;
static int initialized = 0;

/**
 * Initialize the kernel heap
 */
int kheap_init(void) {
    if (initialized) return 0;  /* Already initialized */
    
    /* Create initial free block covering entire heap */
    heap_start = (block_header_t *)KHEAP_START;
    heap_start->size = KHEAP_SIZE;
    heap_start->is_free = 1;
    heap_start->next = NULL;
    heap_start->prev = NULL;
    
    heap_end = heap_start;
    initialized = 1;
    return 0;  /* Success */
}

/**
 * Allocate memory from kernel heap
 */
void *kmalloc(size_t size) {
    if (!initialized || size == 0) return NULL;
    
    /* Align size to 16 bytes */
    size = (size + 15) & ~15;
    size_t total_size = size + sizeof(block_header_t);
    
    /* Find first free block that fits */
    block_header_t *block = heap_start;
    while (block) {
        if (block->is_free && block->size >= total_size) {
            /* Found a suitable block */
            
            /* Split if large enough */
            if (block->size >= total_size + sizeof(block_header_t) + 16) {
                /* Create new free block after allocated portion */
                block_header_t *new_block = (block_header_t *)((uint8_t *)block + total_size);
                new_block->size = block->size - total_size;
                new_block->is_free = 1;
                new_block->next = block->next;
                new_block->prev = block;
                
                if (block->next) {
                    block->next->prev = new_block;
                }
                
                block->next = new_block;
                block->size = total_size;
                
                if (block == heap_end) {
                    heap_end = new_block;
                }
            }
            
            block->is_free = 0;
            return (void *)((uint8_t *)block + sizeof(block_header_t));
        }
        block = block->next;
    }
    
    return NULL;  /* Out of memory */
}

/**
 * Free allocated memory
 */
void kfree(void *ptr) {
    if (!ptr || !initialized) return;
    
    /* Get block header */
    block_header_t *block = (block_header_t *)((uint8_t *)ptr - sizeof(block_header_t));
    
    /* Validate pointer is within heap */
    if ((uint32_t)block < KHEAP_START || (uint32_t)block >= KHEAP_END) {
        return;
    }
    
    block->is_free = 1;
    
    /* Merge with next block if free */
    if (block->next && block->next->is_free) {
        block->size += block->next->size;
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        } else {
            heap_end = block;
        }
    }
    
    /* Merge with previous block if free */
    if (block->prev && block->prev->is_free) {
        block->prev->size += block->size;
        block->prev->next = block->next;
        if (block->next) {
            block->next->prev = block->prev;
        } else {
            heap_end = block->prev;
        }
    }
}

/**
 * Reallocate memory block
 */
void *krealloc(void *ptr, size_t new_size) {
    if (!ptr) return kmalloc(new_size);
    if (new_size == 0) {
        kfree(ptr);
        return NULL;
    }
    
    /* Get current block size */
    block_header_t *block = (block_header_t *)((uint8_t *)ptr - sizeof(block_header_t));
    size_t old_size = block->size - sizeof(block_header_t);
    
    /* If new size fits in current block, just return */
    if (new_size <= old_size) {
        return ptr;
    }
    
    /* Allocate new block and copy */
    void *new_ptr = kmalloc(new_size);
    if (!new_ptr) return NULL;
    
    /* Copy old data */
    uint8_t *src = (uint8_t *)ptr;
    uint8_t *dst = (uint8_t *)new_ptr;
    for (size_t i = 0; i < old_size && i < new_size; i++) {
        dst[i] = src[i];
    }
    
    kfree(ptr);
    return new_ptr;
}

/**
 * Allocate zeroed memory
 */
void *kcalloc(size_t num, size_t size) {
    size_t total = num * size;
    void *ptr = kmalloc(total);
    if (ptr) {
        uint8_t *p = (uint8_t *)ptr;
        for (size_t i = 0; i < total; i++) {
            p[i] = 0;
        }
    }
    return ptr;
}
