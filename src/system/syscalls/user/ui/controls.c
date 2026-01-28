/**
 * UI Controls Syscalls - Ring 3 User Mode Implementations
 */

#include "controls.h"

int syscall_ui_register_button(int owner_pid, int x, int y, int w, int h, const char *label) {
    int result;
    uint32_t wh_packed = (h << 16) | (w & 0xFFFF);
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_UI_REGISTER_BUTTON), "b"(owner_pid), "c"(x), "d"(y), 
          "S"(wh_packed), "D"(label)
        : "memory"
    );
    return result;
}

int syscall_ui_create_button(int window_id, int x, int y, int w, int h, const char *text) {
    int result;
    uint32_t wh_packed = (h << 16) | (w & 0xFFFF);
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_UI_CREATE_BUTTON), "b"(window_id), "c"(x), "d"(y), "S"(wh_packed), "D"(text)
        : "memory"
    );
    return result;
}

int syscall_ui_create_label(int window_id, int x, int y, const char *text) {
    int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_UI_CREATE_LABEL), "b"(window_id), "c"(x), "d"(y), "S"(text)
        : "memory"
    );
    return result;
}

int syscall_ui_poll_event(void *event_ptr) {
    int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_UI_POLL_EVENT), "b"(event_ptr)
        : "memory"
    );
    return result;
}

int syscall_ui_create_icon(int window_id, int x, int y, const char *text) {
    int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_UI_CREATE_ICON), "b"(window_id), "c"(x), "d"(y), "S"(text)
        : "memory"
    );
    return result;
}

int syscall_ui_update_control_text(int control_id, const char *text) {
    int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_UI_UPDATE_CONTROL_TEXT), "b"(control_id), "c"(text)
        : "memory"
    );
    return result;
}
