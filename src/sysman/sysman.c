#include "../syscalls/user_syscalls.h"

void sysman_main_c(void) {
    syscall_puts("Hello from Sysman!\n");
    
    // Infinite loop without HLT
    while(1) {
        // Just loop, don't halt
    }
}