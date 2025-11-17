#include "../syscalls/user_syscalls.h"

void sysman_main_c(void) {
    /* Clear screen */
    syscall_clear();
    
    /* Print welcome message */
    syscall_set_color(15, 1);  /* White on Blue */
    syscall_print_at(20, 0, "=== Welcome to MaahiOS System Manager ===");
    
    syscall_set_color(10, 0);  /* Green on Black */
    syscall_print_at(0, 2, "sysman: System Manager is now running in Ring 3 (user mode)");
    
    syscall_set_color(14, 0);  /* Yellow on Black */
    syscall_print_at(0, 4, "sysman: This is the first user-mode process (PID 1)");
    
    syscall_set_color(11, 0);  /* Light Cyan on Black */
    syscall_print_at(0, 6, "sysman: In the future, this will:");
    syscall_print_at(0, 7, "  - Handle authentication and security");
    syscall_print_at(0, 8, "  - Initialize system services");
    syscall_print_at(0, 9, "  - Launch the Orbit shell");
    
    syscall_set_color(7, 0);  /* Light Gray on Black */
    syscall_print_at(0, 11, "sysman: Currently in idle mode...");
    
    /* Infinite loop - sysman stays alive */
    while(1) {
        /* Idle - in future will wait for events/syscalls */
    }
}