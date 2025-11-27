#include "libgui.h"
#include "../syscalls/user_syscalls.h"

/**
 * Drawing primitive wrappers
 * These wrap the syscalls for convenience
 */

void gui_draw_filled_rect(int x, int y, int width, int height, uint32_t color) {
    syscall_fill_rect(x, y, width, height, color);
}

void gui_draw_rect(int x, int y, int width, int height, uint32_t color) {
    syscall_draw_rect(x, y, width, height, color);
}

void gui_draw_text(int x, int y, const char *text, uint32_t fg, uint32_t bg) {
    syscall_print_at(x, y, text, fg, bg);
}

void gui_clear_screen(uint32_t color) {
    syscall_gfx_clear_color(color);
}
