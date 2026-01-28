/**
 * Window Management Syscalls - Ring 3 User Mode Implementations
 */

#include "window.h"

int syscall_find_window_by_title(const char *title) {
    int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(85), "b"(title)
        : "memory"
    );
    return result;
}

int syscall_get_window_state(int window_id) {
    int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(86), "b"(window_id)
        : "memory"
    );
    return result;
}

int syscall_restore_window(int window_id) {
    int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(87), "b"(window_id)
        : "memory"
    );
    return result;
}

int syscall_focus_window(int window_id) {
    int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(88), "b"(window_id)
        : "memory"
    );
    return result;
}

void syscall_set_window_icon(int window_id, const char *icon_name) {
    asm volatile(
        "int $0x80"
        :
        : "a"(82), "b"(window_id), "c"(icon_name)
        : "memory"
    );
}
