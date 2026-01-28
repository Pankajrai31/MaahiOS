/**
 * Mouse and Input Syscalls - Ring 3 User Mode Implementations
 */

#include "mouse.h"

int syscall_mouse_get_x(void) {
    int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_MOUSE_GET_X)
        : "memory"
    );
    return result;
}

int syscall_mouse_get_y(void) {
    int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_MOUSE_GET_Y)
        : "memory"
    );
    return result;
}

unsigned int syscall_mouse_get_buttons(void) {
    unsigned int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_MOUSE_GET_BUTTONS)
        : "memory"
    );
    return result;
}

int syscall_mouse_get_irq_total(void) {
    int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_MOUSE_GET_IRQ_TOTAL)
        : "memory"
    );
    return result;
}

unsigned int syscall_get_pic_mask(void) {
    unsigned int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_GET_PIC_MASK)
        : "memory"
    );
    return result;
}

void syscall_re_enable_mouse(void) {
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_RE_ENABLE_MOUSE)
        : "memory"
    );
}

int syscall_poll_mouse(void) {
    int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_POLL_MOUSE)
        : "memory"
    );
    return result;
}
