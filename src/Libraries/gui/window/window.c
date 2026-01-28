/**
 * MaahiOS - Window Management Implementation
 */

#include "window.h"
#include "../../core/syscall_helpers.h"

int maahi_create_window(int x, int y, int width, int height, const char *title) {
    int result;
    register int _x __asm__("ebx") = x;
    register int _y __asm__("ecx") = y;
    register int _w __asm__("edx") = width;
    register int _h __asm__("esi") = height;
    
    /* Syscall 40: ui_create_window(x, y, w, h, title, parent=0) */
    __asm__ volatile(
        "push $0\n"          /* parent = 0 (no parent) */
        "push %5\n"          /* title */
        "mov $40, %%eax\n"
        "int $0x80\n"
        "add $8, %%esp\n"
        : "=a"(result)
        : "b"(_x), "c"(_y), "d"(_w), "S"(_h), "r"(title)
        : "memory"
    );
    return result;
}

void maahi_set_window_icon(int window_id, const char *icon_name) {
    register int _wid __asm__("ebx") = window_id;
    register const char *_icon __asm__("ecx") = icon_name;
    
    /* Syscall 62: set_window_icon(window_id, icon_name) */
    __asm__ volatile(
        "mov $62, %%eax\n"
        "int $0x80\n"
        :
        : "b"(_wid), "c"(_icon)
        : "memory"
    );
}
