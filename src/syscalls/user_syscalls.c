// Syscall implementations are in pure assembly (user_syscalls.s)
// This file is empty - all functions implemented in assembly
#include "user_syscalls.h"

/**
 * Ring 3 Syscall Wrappers
 * 
 * Uses the OSDev-recommended approach:
 * - Load parameters directly into registers (EAX, EBX, ECX, EDX)
 * - Use volatile inline asm with explicit register constraints
 * - Trigger INT 0x80
 * - Let the interrupt handler read the registers
 * 
 * Key insight: Don't try to be clever with "memory", "cc" clobbers
 * Just load registers and call INT 0x80
 */

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

void *syscall_alloc_page() {
    void *result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_ALLOC_PAGE)
        : "memory"
    );
    return result;
}

void syscall_free_page(void *addr) {
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_FREE_PAGE), "b"(addr)
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

void syscall_draw_rect(int x, int y, int width, int height, unsigned char color) {
    // Pack parameters: arg3 = width | (height << 8) | (color << 16)
    unsigned int packed = width | (height << 8) | (color << 16);
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_DRAW_RECT), "b"(x), "c"(y), "d"(packed)
        : "memory"
    );
}

void syscall_graphics_mode(void) {
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_GRAPHICS_MODE)
        : "memory"
    );
}

void syscall_put_pixel(int x, int y, unsigned char color) {
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_PUT_PIXEL), "b"(x), "c"(y), "d"(color)
        : "memory"
    );
}

void syscall_clear_gfx(unsigned char color) {
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_CLEAR_GFX), "b"(color)
        : "memory"
    );
}
