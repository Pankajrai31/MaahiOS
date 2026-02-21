/**
 * MaahiOS - Event Implementation
 */

#include "event.h"
#include "../../core/syscall_helpers.h"

int maahi_poll_event(MaahiEvent *event) {
    int result;
    
    /* Syscall 43: ui_poll_event(event_out) */
    __asm__ volatile(
        "mov $43, %%eax\n"
        "mov %1, %%ebx\n"
        "int $0x80\n"
        "mov %%eax, %0\n"
        : "=r"(result)
        : "r"(event)
        : "eax", "ebx", "memory"
    );
    return result;
}

void maahi_yield(void) {
    /* Syscall 31: yield() */
    syscall0(SYSCALL_YIELD);
}
