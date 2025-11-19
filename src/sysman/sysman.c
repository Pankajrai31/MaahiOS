#include "../syscalls/user_syscalls.h"

void sysman_main_c(void) {
    /* Debug: Sysman is running */
    syscall_set_color(14, 0);  // Yellow
    syscall_print_at(0, 10, "[SYSMAN] Running in Ring 3");
    
    /* Get orbit address via syscall and create orbit process */
    syscall_print_at(0, 11, "[SYSMAN] Getting orbit address...");
    unsigned int orbit_addr = syscall_get_orbit_address();
    
    /* Create orbit process via syscall */
    syscall_print_at(0, 12, "[SYSMAN] Creating orbit process...");
    int orbit_pid = syscall_create_process(orbit_addr);
    
    if (orbit_pid > 0) {
        syscall_print_at(0, 13, "[SYSMAN] Orbit created! Waiting...");
    } else {
        syscall_print_at(0, 13, "[SYSMAN] ERROR: Failed to create orbit");
    }
    
    /* Infinite loop - sysman stays alive */
    while(1) {
        /* Busy wait - scheduler will preempt us */
    }
}