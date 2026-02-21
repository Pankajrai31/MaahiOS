/**
 * MaahiOS - Icon Implementation
 */

#include "icon.h"
#include "../../core/syscall_helpers.h"

int maahi_create_icon(int window_id, int x, int y, const char *text) {
    int result;
    register int _wid __asm__("ebx") = window_id;
    register int _x __asm__("ecx") = x;
    register int _y __asm__("edx") = y;
    register const char *_text __asm__("esi") = text;
    
    /* Syscall 54: ui_create_icon(window_id, x, y, text) */
    __asm__ volatile(
        "mov $54, %%eax\n"
        "int $0x80\n"
        : "=a"(result)
        : "b"(_wid), "c"(_x), "d"(_y), "S"(_text)
    );
    return result;
}
