#ifndef USER_SYSCALLS_H
#define USER_SYSCALLS_H

/* Include syscall number definitions */
#include "syscall_numbers.h"

/**
 * Ring 3 Syscall Interface
 * 
 * These functions are callable from Ring 3 (user mode)
 * They trigger INT 0x80 to request kernel services
 */

/**
 * syscall_putchar - Print a single character via kernel
 */
void syscall_putchar(char c);

/**
 * syscall_puts - Print a null-terminated string via kernel
 */
void syscall_puts(const char* str);

/**
 * syscall_putint - Print an integer via kernel
 */
void syscall_putint(int num);

/**
 * syscall_exit - Terminate program via kernel
 */
void syscall_exit(int code);

/**
 * syscall_alloc_page - Allocate a 4KB physical page
 * Returns: Address of allocated page, or 0 if no memory available
 */
void *syscall_alloc_page();

/**
 * syscall_free_page - Free a previously allocated page
 */
void syscall_free_page(void *addr);

/**
 * syscall_clear - Clear the screen
 */
void syscall_clear();

#endif // USER_SYSCALLS_H