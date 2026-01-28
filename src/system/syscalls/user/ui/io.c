/**
 * I/O Syscalls - Ring 3 User Mode Implementations
 */

#include "io.h"

void syscall_putchar(char c) {
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_PUTCHAR), "b"(c)
        : "memory"
    );
}

void syscall_puts(const char* str) {
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_PUTS), "b"(str)
        : "memory"
    );
}

void syscall_putint(int num) {
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_PUTINT), "b"(num)
        : "memory"
    );
}

void syscall_exit(int code) {
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_EXIT), "b"(code)
        : "memory"
    );
}

void syscall_clear() {
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_CLEAR)
        : "memory"
    );
}

void syscall_set_color(unsigned char fg, unsigned char bg) {
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_SET_COLOR), "b"(fg), "c"(bg)
        : "memory"
    );
}
