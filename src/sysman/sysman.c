#include "../syscalls/user_syscalls.h"
#include "../libgui/libgui.h"

void sysman_main_c(void) {
    syscall_puts("[SYSMAN] Entry\n");
    
    // Get orbit module address from kernel
    unsigned int orbit_addr = syscall_get_orbit_address();
    
    if (orbit_addr == 0) {
        gui_draw_text(450, 420, "ERROR: ORBIT NOT LOADED", 0xFF0000, 0);
        while(1) __asm__ volatile("hlt");
    }
    
    syscall_puts("[SYSMAN] Orbit address: 0x");
    // Simple hex print inline
    for (int i = 28; i >= 0; i -= 4) {
        char c = "0123456789ABCDEF"[(orbit_addr >> i) & 0xF];
        syscall_putchar(c);
    }
    syscall_puts("\n");
    
    // Create orbit as separate process (process 2)
    syscall_puts("[SYSMAN] Creating orbit...\n");
    int orbit_pid = syscall_create_process(orbit_addr);
    
    if (orbit_pid < 0) {
        gui_draw_text(450, 420, "ERROR: FAILED TO START ORBIT", 0xFF0000, 0);
        while(1) __asm__ volatile("hlt");
    }
    
    syscall_puts("[SYSMAN] Orbit started as PID ");
    syscall_putchar('0' + orbit_pid);
    syscall_puts("\n");
    
    // Sysman's job is done - just idle forever
    // Timer will switch between processes automatically
    syscall_puts("[SYSMAN] Idle loop, timer handles multitasking\n");
    while(1) {
        // Empty loop - preemptive multitasking via timer
        // No yield needed - timer IRQ switches processes
        __asm__ volatile("nop");
    }
}