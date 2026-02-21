/**
 * MaahiOS - Graphics Implementation
 */

#include "graphics.h"
#include "../../core/syscall_helpers.h"

void maahi_fill_rect(int x, int y, int width, int height, unsigned int color) {
    /* Syscall 23: gfx_fill_rect(x, y, w, h, color) */
    __asm__ volatile(
        "push %4\n"
        "mov $23, %%eax\n"
        "int $0x80\n"
        "add $4, %%esp\n"
        :
        : "b"(x), "c"(y), "d"(width), "S"(height), "r"(color)
        : "eax", "memory"
    );
}

void maahi_draw_rect(int x, int y, int width, int height, unsigned int color) {
    /* Syscall 24: gfx_draw_rect(x, y, w, h, color) */
    __asm__ volatile(
        "push %4\n"
        "mov $24, %%eax\n"
        "int $0x80\n"
        "add $4, %%esp\n"
        :
        : "b"(x), "c"(y), "d"(width), "S"(height), "r"(color)
        : "eax", "memory"
    );
}

void maahi_clear_screen(unsigned int color) {
    /* Syscall 26: gfx_clear_color(rgb) */
    syscall1v(SYSCALL_GFX_CLEAR_COLOR, (int)color);
}
