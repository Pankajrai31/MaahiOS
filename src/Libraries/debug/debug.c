/**
 * MaahiOS - Debug and Output Implementation
 */

#include "debug.h"
#include "../core/syscall_helpers.h"

void maahi_print(const char *str) {
    /* Syscall 2: puts(str) */
    __asm__ volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_PUTS), "b"(str)
        : "memory"
    );
}

void maahi_putchar(char c) {
    /* Syscall 1: putchar(c) */
    syscall1v(SYSCALL_PUTCHAR, (int)c);
}

void maahi_debug_dump_resources(void) {
    syscall0(90);  /* SYSCALL_DEBUG_DUMP_RESOURCES */
}

int maahi_launch_file_manager(void) {
    int result;
    __asm__ volatile(
        "mov $52, %%eax\n"
        "int $0x80\n"
        : "=a"(result)
        :
        : "memory"
    );
    return result;
}
