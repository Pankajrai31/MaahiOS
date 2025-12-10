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

/**
 * Graphics Syscalls - Simple text-mode-like API
 * Just like printf() - no hex values, no memory addresses
 */

/* Color constants - use these instead of hex values */
#define COLOR_BLACK     0
#define COLOR_WHITE     1
#define COLOR_RED       2
#define COLOR_GREEN     3
#define COLOR_BLUE      4
#define COLOR_YELLOW    5
#define COLOR_CYAN      6
#define COLOR_MAGENTA   7

/**
 * gfx_putc - Print a single character at cursor
 * c: character to print
 */
void gfx_putc(char c);

/**
 * gfx_puts - Print string at cursor (like puts())
 * str: null-terminated string
 */
void gfx_puts(const char *str);

/**
 * gfx_clear - Clear screen
 */
void gfx_clear(void);

/**
 * gfx_set_color - Set text colors for next print
 * fg: foreground color (use COLOR_xxx constants)
 * bg: background color (use COLOR_xxx constants)
 */
void gfx_set_color(int fg, int bg);

/**
 * Graphics primitive syscalls for desktop/GUI
 */
void syscall_fill_rect(int x, int y, int width, int height, unsigned int color);
void syscall_draw_rect(int x, int y, int width, int height, unsigned int color);
void syscall_print_at(int x, int y, const char *str, unsigned int fg, unsigned int bg);
void syscall_gfx_clear_color(unsigned int rgb_color);
void syscall_draw_bmp(int x, int y, unsigned int bmp_data_addr);

/**
 * Mouse syscalls
 */
int syscall_mouse_get_x(void);
int syscall_mouse_get_y(void);
unsigned int syscall_mouse_get_buttons(void);

/**
 * Scheduler syscalls
 */
void syscall_yield(void);

/**
 * Debug syscalls
 */
int syscall_mouse_get_irq_total(void);
int syscall_poll_mouse(void);  // Returns 1 if polled data, 0 if no data
unsigned int syscall_get_pic_mask(void);
void syscall_re_enable_mouse(void);

/**
 * Cursor compositor syscall - read pixel from framebuffer
 */
unsigned int syscall_read_pixel(int x, int y);

#endif // USER_SYSCALLS_H