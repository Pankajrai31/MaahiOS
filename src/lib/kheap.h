/*
 * Kernel Heap Allocator Header
 * 
 * Dynamic memory allocation for kernel (Ring 0)
 * Similar to user heap but uses PMM directly instead of syscalls
 */

#ifndef KHEAP_H
#define KHEAP_H

#include <stddef.h>

/* Initialize kernel heap */
void kheap_init(void);

/* Allocate memory from kernel heap */
void* kmalloc(size_t size);

/* Free memory back to kernel heap */
void kfree(void* ptr);

/* Reallocate memory (resize) */
void* krealloc(void* ptr, size_t new_size);

/* Allocate and zero memory */
void* kcalloc(size_t count, size_t size);

/* Get heap statistics */
void kheap_stats(unsigned int *total_pages, unsigned int *used_bytes, unsigned int *free_bytes);

#endif /* KHEAP_H */
