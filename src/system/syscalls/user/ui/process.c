/**
 * Process Management Syscalls - Ring 3 User Mode Implementations
 */

#include "process.h"

int syscall_create_process(unsigned int entry_point) {
    int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_CREATE_PROCESS), "b"(entry_point)
        : "memory"
    );
    return result;
}

int syscall_kill_process(int pid) {
    int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_KILL_PROCESS), "b"(pid)
        : "memory"
    );
    return result;
}

int syscall_get_orbit_address(void) {
    int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_GET_ORBIT_ADDR)
        : "memory"
    );
    return result;
}

int syscall_get_uimanager_address(void) {
    int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_GET_UIMANAGER_ADDR)
        : "memory"
    );
    return result;
}

int syscall_get_current_pid(void) {
    int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_GET_CURRENT_PID)
        : "memory"
    );
    return result;
}

void syscall_yield(void) {
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_YIELD)
        : "memory"
    );
}
