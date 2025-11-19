#include "../syscalls/user_syscalls.h"

void orbit_main_c(void) {
    syscall_clear();
    syscall_set_color(11, 0);  // Cyan on black
    syscall_print_at(30, 12, "Welcome to Orbit");
    
    /* Infinite loop - orbit stays alive */
    while(1) {
        /* Busy wait - scheduler will preempt us */
    }
}
