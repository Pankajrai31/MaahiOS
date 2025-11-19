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

/**
 * syscall_print_at - Print string at specific position
 * x: 0-79, y: 0-24, str: null-terminated string
 */
void syscall_print_at(int x, int y, const char *str);

/**
 * syscall_set_cursor - Set cursor position
 * x: 0-79, y: 0-24
 */
void syscall_set_cursor(int x, int y);

/**
 * syscall_draw_box - Draw box border with '=' and '|'
 * x, y: position, width, height: size in characters
 */
void syscall_draw_box(int x, int y, int width, int height);

/**
 * syscall_create_process - Create new process
 * entry_point: Address where process starts execution
 * Returns: Process ID (PID) on success, -1 on failure
 */
int syscall_create_process(unsigned int entry_point);

/**
 * syscall_get_orbit_address - Get orbit module address from kernel
 * Returns: Address of orbit.bin loaded by GRUB
 */
int syscall_get_orbit_address(void);

#endif // USER_SYSCALLS_H