/**
 * MaahiOS - Text Rendering Implementation
 */

#include "text.h"
#include "../../core/syscall_helpers.h"

void maahi_draw_text(int x, int y, const char *text, unsigned int fg, unsigned int bg) {
    /* Syscall 25: gfx_print_at(x, y, str, fg, bg) */
    __asm__ volatile(
        "push %4\n"
        "push %3\n"
        "mov $25, %%eax\n"
        "int $0x80\n"
        "add $8, %%esp\n"
        :
        : "b"(x), "c"(y), "d"(text), "r"(fg), "r"(bg)
        : "eax", "memory"
    );
}
