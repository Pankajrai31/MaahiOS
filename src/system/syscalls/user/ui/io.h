#ifndef USER_IO_SYSCALLS_H
#define USER_IO_SYSCALLS_H

#include "../../syscall_numbers.h"

/**
 * I/O Syscalls - Ring 3 User Mode
 * Basic input/output operations
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
 * syscall_clear - Clear the screen
 */
void syscall_clear();

/**
 * syscall_set_color - Set foreground and background text color
 */
void syscall_set_color(unsigned char fg, unsigned char bg);

#endif // USER_IO_SYSCALLS_H
