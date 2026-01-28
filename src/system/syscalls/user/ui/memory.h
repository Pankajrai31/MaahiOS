#ifndef USER_MEMORY_SYSCALLS_H
#define USER_MEMORY_SYSCALLS_H

#include "../../syscall_numbers.h"

/**
 * Memory Management Syscalls - Ring 3 User Mode
 */

/**
 * syscall_alloc_page - Allocate a 4KB physical page
 * Returns: Address of allocated page, or 0 if no memory available
 */
void *syscall_alloc_page();

/**
 * syscall_allocate_memory - Allocate multiple pages for large buffer
 * Returns: Address of allocated memory, or 0 if failed
 */
void *syscall_allocate_memory(unsigned int size);

/**
 * syscall_free_page - Free a previously allocated page
 */
void syscall_free_page(void *addr);

#endif // USER_MEMORY_SYSCALLS_H
