#ifndef HEAP_H
#define HEAP_H

#include <stddef.h>

/* Initialize heap allocator */
void heap_init(void);

/* Allocate memory from heap */
void* malloc(size_t size);

/* Free memory back to heap */
void free(void* ptr);

/* Get heap statistics for debugging */
void heap_stats(unsigned int *total_pages, unsigned int *used_bytes, unsigned int *free_bytes);

#endif /* HEAP_H */
