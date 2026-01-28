#ifndef USER_GRAPHICS_SYSCALLS_H
#define USER_GRAPHICS_SYSCALLS_H

#include "../../syscall_numbers.h"

/* Color constants */
#define COLOR_BLACK     0
#define COLOR_WHITE     1
#define COLOR_RED       2
#define COLOR_GREEN     3
#define COLOR_BLUE      4
#define COLOR_YELLOW    5
#define COLOR_CYAN      6
#define COLOR_MAGENTA   7

/**
 * Graphics Syscalls - Ring 3 User Mode
 */

void gfx_putc(char c);
void gfx_puts(const char *str);
void gfx_clear(void);
void gfx_set_color(int fg, int bg);
void syscall_fill_rect(int x, int y, int width, int height, unsigned int color);
void syscall_draw_rect(int x, int y, int width, int height, unsigned int color);
void syscall_print_at(int x, int y, const char *str, unsigned int fg, unsigned int bg);
void syscall_gfx_clear_color(unsigned int rgb_color);
void syscall_draw_bmp(int x, int y, unsigned int bmp_data_addr);
unsigned int syscall_read_pixel(int x, int y);

#endif // USER_GRAPHICS_SYSCALLS_H
