/**
 * MaahiOS - Label Implementation
 */

#include "label.h"
#include "../../core/syscall_helpers.h"

int maahi_create_label(int window_id, int x, int y, const char *text) {
    int result;
    register int _wid __asm__("ebx") = window_id;
    register int _x __asm__("ecx") = x;
    register int _y __asm__("edx") = y;
    register const char *_text __asm__("esi") = text;
    
    /* Syscall 42: ui_create_label(window_id, x, y, text) */
    __asm__ volatile(
        "mov $42, %%eax\n"
        "int $0x80\n"
        : "=a"(result)
        : "b"(_wid), "c"(_x), "d"(_y), "S"(_text)
    );
    return result;
}

int maahi_update_text(int control_id, const char *text) {
    int result;
    register int _id __asm__("ebx") = control_id;
    register const char *_text __asm__("ecx") = text;
    
    /* Syscall 63: control_set_text(control_id, text) */
    __asm__ volatile(
        "mov $63, %%eax\n"
        "int $0x80\n"
        : "=a"(result)
        : "b"(_id), "c"(_text)
        : "memory"
    );
    return result;
}
