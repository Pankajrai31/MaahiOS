/**
 * MaahiOS - Internal Syscall Helpers
 * 
 * Low-level syscall wrappers used by all library components.
 * Applications should NOT include this directly - use maahi.h instead.
 */

#ifndef SYSCALL_HELPERS_H
#define SYSCALL_HELPERS_H

#include "../../system/syscalls/syscall_numbers.h"

/* Generic syscall with no return value */
static inline void syscall0(int num) {
    __asm__ volatile("int $0x80" : : "a"(num) : "memory");
}

/* Syscall with 1 arg, no return */
static inline void syscall1v(int num, int a1) {
    __asm__ volatile("int $0x80" : : "a"(num), "b"(a1) : "memory");
}

/* Syscall with 1 arg, returns int */
static inline int syscall1(int num, int a1) {
    int result;
    __asm__ volatile("int $0x80" : "=a"(result) : "a"(num), "b"(a1) : "memory");
    return result;
}

/* Syscall with 2 args, returns int */
static inline int syscall2(int num, int a1, int a2) {
    int result;
    __asm__ volatile("int $0x80" : "=a"(result) : "a"(num), "b"(a1), "c"(a2) : "memory");
    return result;
}

/* Syscall with 4 args, no return */
static inline void syscall4v(int num, int a1, int a2, int a3, int a4) {
    __asm__ volatile(
        "int $0x80"
        :
        : "a"(num), "b"(a1), "c"(a2), "d"(a3), "S"(a4)
        : "memory"
    );
}

/* Syscall with 5 args, no return (for fill_rect with color) */
static inline void syscall5v(int num, int a1, int a2, int a3, int a4, int a5) {
    __asm__ volatile(
        "push %5\n"
        "int $0x80\n"
        "add $4, %%esp\n"
        :
        : "a"(num), "b"(a1), "c"(a2), "d"(a3), "S"(a4), "r"(a5)
        : "memory"
    );
}

#endif /* SYSCALL_HELPERS_H */
