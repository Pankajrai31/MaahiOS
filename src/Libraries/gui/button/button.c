/**
 * MaahiOS - Button Implementation
 */

#include "button.h"
#include "../../core/syscall_helpers.h"

int maahi_create_button(int window_id, int x, int y, int width, int height, const char *text) {
    int result;
    register int _wid __asm__("ebx") = window_id;
    register int _x __asm__("ecx") = x;
    register int _y __asm__("edx") = y;
    register int _w __asm__("esi") = width;
    
    /* Syscall 41: ui_create_button(window_id, x, y, w, h, text) */
    __asm__ volatile(
        "push %6\n"
        "push %5\n"
        "mov $41, %%eax\n"
        "int $0x80\n"
        "add $8, %%esp\n"
        : "=a"(result)
        : "b"(_wid), "c"(_x), "d"(_y), "S"(_w), "r"(height), "r"(text)
        : "memory"
    );
    return result;
}

int maahi_button_create(int window_id, int x, int y, int width, int height,
                        const char *text, int type, int size) {
    // For now, just call existing maahi_create_button
    // TODO: Pass type and size as metadata when syscall is updated
    return maahi_create_button(window_id, x, y, width, height, text);
}
