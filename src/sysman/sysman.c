#include "../syscalls/user_syscalls.h"
#include "../libgui/libgui.h"

void sysman_main_c(void) {
    // Clear screen to black
    
    // Get orbit address
    unsigned int orbit_addr = syscall_get_orbit_address();
    
    if (orbit_addr == 0) {
        gui_draw_text(450, 420, "ERROR: ORBIT NOT LOADED", 0xFF0000, 0);
        while(1) __asm__ volatile("hlt");
    }
    
    // Create orbit as separate process (process 2)
    int orbit_pid = syscall_create_process(orbit_addr);
    
    if (orbit_pid < 0) {
        gui_draw_text(450, 420, "ERROR: FAILED TO START ORBIT", 0xFF0000, 0);
        while(1) __asm__ volatile("hlt");
    }
    
    // Sysman continues running as system tray
    gui_clear_screen(0x000000);
    gui_draw_text(10, 10, "Sysman running (PID 1)", 0x00FF00, 0);
    
    // Yield to let orbit start
    syscall_yield();
    
    while(1) {
        __asm__ volatile("hlt");
    }
}