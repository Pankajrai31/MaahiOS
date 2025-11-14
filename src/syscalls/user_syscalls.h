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

/**
 * syscall_set_color - Set foreground and background text color
 * fg: 0-15 (foreground color)
 * bg: 0-15 (background color)
 */
void syscall_set_color(unsigned char fg, unsigned char bg);

/**
 * syscall_draw_rect - Draw a filled rectangle
 * x, y: position (0-79 for x, 0-24 for y)
 * width, height: size in characters
 * color: attribute byte (fg | (bg << 4))
 */
void syscall_draw_rect(int x, int y, int width, int height, unsigned char color);

/**
 * syscall_graphics_mode - Switch to 320x200 graphics mode
 */
void syscall_graphics_mode(void);

/**
 * syscall_put_pixel - Draw a pixel in graphics mode
 * x: 0-319, y: 0-199, color: 0-255
 */
void syscall_put_pixel(int x, int y, unsigned char color);

/**
 * syscall_clear_gfx - Clear graphics screen to color
 * color: 0-255
 */
void syscall_clear_gfx(unsigned char color);

#endif // USER_SYSCALLS_H