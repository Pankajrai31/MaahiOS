#include "../syscalls/user_syscalls.h"
#include "../libgui/libgui.h"

void sysman_main_c(void) {
    // Clear screen to black
    gui_clear_screen(0x000000);
    
    // Draw blue shadow box (480x80 at center-ish position)
    gui_draw_filled_rect(404, 364, 480, 80, 0x0000AA);  // Dark blue shadow
    
    // Draw grey main box on top (480x80 at center)
    gui_draw_filled_rect(400, 360, 480, 80, 0x808080);  // Grey box
    
    // Display "Starting Orbit..." message in center of grey box
    gui_draw_text(480, 395, "Starting Orbit...", 0x000000, 0);  // Black text on grey
    
    // Display version at bottom right
    gui_draw_text(850, 730, "MaahiOS v0.1", 0xFFFFFF, 0);  // White text
    
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
    while(1) {
        __asm__ volatile("hlt");
    }
}