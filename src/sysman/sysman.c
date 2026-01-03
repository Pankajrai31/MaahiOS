#include "../syscalls/user_syscalls.h"
#include "../libgui/libgui.h"

void sysman_main_c(void) {
    syscall_puts("[SYSMAN] Entry\n");
    
    // Get UIManager module address from kernel
    unsigned int uimanager_addr = syscall_get_uimanager_address();
    
    if (uimanager_addr == 0) {
        gui_draw_text(450, 420, "ERROR: UIMANAGER NOT LOADED", 0xFF0000, 0);
        while(1) __asm__ volatile("hlt");
    }
    
    syscall_puts("[SYSMAN] UIManager address: 0x");
    for (int i = 28; i >= 0; i -= 4) {
        char c = "0123456789ABCDEF"[(uimanager_addr >> i) & 0xF];
        syscall_putchar(c);
    }
    syscall_puts("\n");
    
    // Create UIManager as process 2 (window server)
    syscall_puts("[SYSMAN] Creating UIManager...\n");
    int uimanager_pid = syscall_create_process(uimanager_addr);
    
    if (uimanager_pid < 0) {
        gui_draw_text(450, 420, "ERROR: FAILED TO START UIMANAGER", 0xFF0000, 0);
        while(1) __asm__ volatile("hlt");
    }
    
    syscall_puts("[SYSMAN] UIManager started as PID ");
    syscall_putchar('0' + uimanager_pid);
    syscall_puts("\n");
    
    // Clear screen with dark blue background (framebuffer now owned by UIManager)
    syscall_puts("[SYSMAN] Clearing screen for Orbit...\n");
    syscall_fill_rect(0, 0, 800, 600, 0x001020);
    
    // Get orbit module address from kernel
    unsigned int orbit_addr = syscall_get_orbit_address();
    
    if (orbit_addr == 0) {
        gui_draw_text(450, 420, "ERROR: ORBIT NOT LOADED", 0xFF0000, 0);
        while(1) __asm__ volatile("hlt");
    }
    
    syscall_puts("[SYSMAN] Orbit address: 0x");
    for (int i = 28; i >= 0; i -= 4) {
        char c = "0123456789ABCDEF"[(orbit_addr >> i) & 0xF];
        syscall_putchar(c);
    }
    syscall_puts("\n");
    
    // Create orbit as process 3 (desktop shell)
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