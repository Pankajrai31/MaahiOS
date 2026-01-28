/**
 * Graphics Syscalls - Ring 3 User Mode Implementations
 */

#include "graphics.h"
#include <stdint.h>

void gfx_putc(char c) {
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_GFX_PUTC), "b"((unsigned int)c)
        : "memory"
    );
}

void gfx_puts(const char *str) {
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_GFX_PUTS), "b"(str)
        : "memory"
    );
}

void gfx_clear(void) {
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_GFX_CLEAR)
        : "memory"
    );
}

void gfx_set_color(int fg, int bg) {
    // Convert color constants to RGB values
    static const unsigned int color_map[] = {
        0x00000000,  // BLACK
        0x00FFFFFF,  // WHITE
        0x00FF0000,  // RED
        0x0000FF00,  // GREEN
        0x000000FF,  // BLUE
        0x00FFFF00,  // YELLOW
        0x0000FFFF,  // CYAN
        0x00FF00FF   // MAGENTA
    };
    
    unsigned int fg_rgb = (fg >= 0 && fg < 8) ? color_map[fg] : color_map[1];
    unsigned int bg_rgb = (bg >= 0 && bg < 8) ? color_map[bg] : color_map[0];
    
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_GFX_SET_COLOR), "b"(fg_rgb), "c"(bg_rgb)
        : "memory"
    );
}

void syscall_fill_rect(int x, int y, int width, int height, unsigned int color) {
    unsigned int packed_wh = ((unsigned int)height << 16) | ((unsigned int)width & 0xFFFF);
    
    asm volatile(
        "movl $23, %%eax\n\t"
        "movl %[x], %%ebx\n\t"
        "movl %[y], %%ecx\n\t"
        "movl %[packed], %%edx\n\t"
        "movl %[color], %%esi\n\t"
        "int $0x80\n\t"
        :
        : [x] "rm" (x), [y] "rm" (y), [packed] "rm" (packed_wh), [color] "rm" (color)
        : "eax", "ebx", "ecx", "edx", "esi", "memory"
    );
}

void syscall_draw_rect(int x, int y, int width, int height, unsigned int color) {
    asm volatile(
        "pushl %[color]\n\t"
        "pushl %[height]\n\t"
        "movl $24, %%eax\n\t"
        "movl %[x], %%ebx\n\t"
        "movl %[y], %%ecx\n\t"
        "movl %[width], %%edx\n\t"
        "int $0x80\n\t"
        "addl $8, %%esp\n\t"
        :
        : [x] "rm" (x), [y] "rm" (y), [width] "rm" (width), [height] "rm" (height), [color] "rm" (color)
        : "eax", "ebx", "ecx", "edx", "memory"
    );
}

void syscall_print_at(int x, int y, const char *str, unsigned int fg, unsigned int bg) {
    asm volatile(
        "pushl %[bg]\n\t"
        "pushl %[fg]\n\t"
        "movl $25, %%eax\n\t"
        "movl %[x], %%ebx\n\t"
        "movl %[y], %%ecx\n\t"
        "movl %[str], %%edx\n\t"
        "int $0x80\n\t"
        "addl $8, %%esp\n\t"
        :
        : [x] "rm" (x), [y] "rm" (y), [str] "rm" (str), [fg] "rm" (fg), [bg] "rm" (bg)
        : "eax", "ebx", "ecx", "edx", "memory"
    );
}

void syscall_gfx_clear_color(unsigned int rgb_color) {
    asm volatile(
        "movl $26, %%eax\n\t"
        "movl %[color], %%ebx\n\t"
        "int $0x80\n\t"
        :
        : [color] "rm" (rgb_color)
        : "eax", "ebx", "memory"
    );
}

void syscall_draw_bmp(int x, int y, unsigned int bmp_data_addr) {
    asm volatile(
        "movl $27, %%eax\n\t"
        "movl %[x], %%ebx\n\t"
        "movl %[y], %%ecx\n\t"
        "movl %[bmp], %%edx\n\t"
        "int $0x80\n\t"
        :
        : [x] "rm" (x), [y] "rm" (y), [bmp] "rm" (bmp_data_addr)
        : "eax", "ebx", "ecx", "edx", "memory"
    );
}

unsigned int syscall_read_pixel(int x, int y) {
    unsigned int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_READ_PIXEL), "b"(x), "c"(y)
        : "memory"
    );
    return result;
}
