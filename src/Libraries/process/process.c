/**
 * MaahiOS - Process Management Implementation
 */

#include "process.h"
#include "../core/syscall_helpers.h"

int maahi_create_process(unsigned int entry_point) {
    /* Syscall 17: create_process(entry_point) */
    return syscall1(SYSCALL_CREATE_PROCESS, (int)entry_point);
}

unsigned int maahi_get_uimanager_address(void) {
    int result;
    /* Syscall 38: get_uimanager_address() */
    __asm__ volatile(
        "mov $38, %%eax\n"
        "int $0x80\n"
        : "=a"(result)
        :
        : "memory"
    );
    return (unsigned int)result;
}

unsigned int maahi_get_orbit_address(void) {
    int result;
    /* Syscall 18: get_orbit_address() */
    __asm__ volatile(
        "mov $18, %%eax\n"
        "int $0x80\n"
        : "=a"(result)
        :
        : "memory"
    );
    return (unsigned int)result;
}
