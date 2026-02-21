/**
 * MaahiOS - List Control Implementation
 */

#include "list.h"
#include "../../core/syscall_helpers.h"

int maahi_create_list(int window_id, int x, int y, int width, int height, const char *items) {
    int result;
    register int _wid __asm__("ebx") = window_id;
    register int _x __asm__("ecx") = x;
    register int _y __asm__("edx") = y;
    register int _w __asm__("esi") = width;
    
    /* Syscall 76: ui_create_list(window_id, x, y, w, h, items) */
    __asm__ volatile(
        "push %6\n"
        "push %5\n"
        "mov $76, %%eax\n"
        "int $0x80\n"
        "add $8, %%esp\n"
        : "=a"(result)
        : "b"(_wid), "c"(_x), "d"(_y), "S"(_w), "r"(height), "r"(items)
        : "memory"
    );
    return result;
}
