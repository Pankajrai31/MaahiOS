/**
 * MaahiOS Kernel Heap Allocator
 * 
 * Simple allocator for dynamic kernel memory.
 * Provides kmalloc/kfree/krealloc/kcalloc.
 */

#ifndef KHEAP_H
#define KHEAP_H

#include <stddef.h>

/**
 * Initialize the kernel heap.
 * @return 0 on success, non-zero on failure
 */
int kheap_init(void);

/**
 * Allocate kernel memory.
 * @param size Bytes to allocate
 * @return Pointer to allocated memory, or NULL on failure
 */
void *kmalloc(size_t size);

/**
 * Free kernel memory.
 * @param ptr Pointer previously returned by kmalloc/krealloc/kcalloc
 */
void kfree(void *ptr);

/**
 * Reallocate kernel memory.
 * @param ptr  Existing allocation (or NULL for new allocation)
 * @param new_size New size in bytes
 * @return Pointer to reallocated memory, or NULL on failure
 */
void *krealloc(void *ptr, size_t new_size);

/**
 * Allocate and zero-initialize kernel memory.
 * @param num  Number of elements
 * @param size Size of each element
 * @return Pointer to zeroed memory, or NULL on failure
 */
void *kcalloc(size_t num, size_t size);

#endif /* KHEAP_H */
