/**
 * MaahiOS - Syscall Helpers
 * 
 * Low-level syscall wrappers for ALL user-space code.
 * Includes syscall numbers + inline asm wrappers.
 * 
 * Include this ONE file — never redefine syscall wrappers elsewhere.
 *
 * IMPORTANT: The kernel syscall handler (int 0x80) does NOT preserve
 * ECX or EDX. Only EBX, ESI, EDI, EBP are callee-saved.
 * EAX is used for the return value.
 * All asm constraints below must reflect this.
 */

#ifndef SYSCALL_HELPERS_H
#define SYSCALL_HELPERS_H

#include "../../syscalls/syscall_numbers.h"

/*=============================================================================
 * Syscall wrappers (return int)
 *===========================================================================*/

static inline int syscall0(int num) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num) : "memory", "ecx", "edx");
    return ret;
}

static inline int syscall1(int num, int a1) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(a1) : "memory", "ecx", "edx");
    return ret;
}

static inline int syscall2(int num, int a1, int a2) {
    int ret;
    int _ecx;
    __asm__ volatile("int $0x80"
        : "=a"(ret), "=c"(_ecx)
        : "a"(num), "b"(a1), "1"(a2)
        : "memory", "edx");
    return ret;
}

static inline int syscall3(int num, int a1, int a2, int a3) {
    int ret;
    int _ecx, _edx;
    __asm__ volatile("int $0x80"
        : "=a"(ret), "=c"(_ecx), "=d"(_edx)
        : "a"(num), "b"(a1), "1"(a2), "2"(a3)
        : "memory");
    return ret;
}

static inline int syscall4(int num, int a1, int a2, int a3, int a4) {
    int ret;
    int _ecx, _edx;
    __asm__ volatile("int $0x80"
        : "=a"(ret), "=c"(_ecx), "=d"(_edx)
        : "a"(num), "b"(a1), "1"(a2), "2"(a3), "S"(a4)
        : "memory");
    return ret;
}

/*=============================================================================
 * Fire-and-forget variants (no return value)
 *===========================================================================*/

static inline void syscall0v(int num) {
    __asm__ volatile("int $0x80" : "+a"(num) : : "memory", "ecx", "edx");
}

static inline void syscall1v(int num, int a1) {
    __asm__ volatile("int $0x80" : "+a"(num) : "b"(a1) : "memory", "ecx", "edx");
}

static inline void syscall4v(int num, int a1, int a2, int a3, int a4) {
    int _ecx, _edx;
    __asm__ volatile("int $0x80"
        : "+a"(num), "=c"(_ecx), "=d"(_edx)
        : "b"(a1), "1"(a2), "2"(a3), "S"(a4)
        : "memory");
}

#endif /* SYSCALL_HELPERS_H */
