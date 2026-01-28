/**
 * MaahiOS System Manager (sysman)
 * 
 * First ring-3 process. Responsible for:
 *   - Starting UIManager (window server)
 *   - Starting Orbit (desktop shell)
 *   - Then idling forever (timer handles multitasking)
 */

#include "../Libraries/maahi.h"

void sysman_main_c(void) {
    maahi_print("[SYSMAN] Entry\n");
    
    // Get UIManager module address from kernel
    unsigned int uimanager_addr = maahi_get_uimanager_address();
    
    if (uimanager_addr == 0) {
        maahi_draw_text(450, 420, "ERROR: UIMANAGER NOT LOADED", 0xFF0000, 0);
        while(1) __asm__ volatile("hlt");
    }
    
    maahi_print("[SYSMAN] UIManager address: 0x");
    for (int i = 28; i >= 0; i -= 4) {
        char c = "0123456789ABCDEF"[(uimanager_addr >> i) & 0xF];
        maahi_putchar(c);
    }
    maahi_print("\n");
    
    // Create UIManager as process 2 (window server)
    maahi_print("[SYSMAN] Creating UIManager...\n");
    int uimanager_pid = maahi_create_process(uimanager_addr);
    
    __asm__ volatile(
        "pushl %%eax\n"
        "pushl %%edx\n"
        "movl $0x3F8, %%edx\n"
        "movb $'C', %%al\n" "outb %%al, %%dx\n"
        "movb $'1', %%al\n" "outb %%al, %%dx\n"
        "movb $'\\n', %%al\n" "outb %%al, %%dx\n"
        "popl %%edx\n"
        "popl %%eax\n"
        ::: "memory"
    );
    
    if (uimanager_pid < 0) {
        maahi_draw_text(450, 420, "ERROR: FAILED TO START UIMANAGER", 0xFF0000, 0);
        while(1) __asm__ volatile("hlt");
    }
    
    __asm__ volatile(
        "pushl %%eax\n"
        "pushl %%edx\n"
        "movl $0x3F8, %%edx\n"
        "movb $'C', %%al\n" "outb %%al, %%dx\n"
        "movb $'2', %%al\n" "outb %%al, %%dx\n"
        "movb $'\\n', %%al\n" "outb %%al, %%dx\n"
        "popl %%edx\n"
        "popl %%eax\n"
        ::: "memory"
    );
    
    maahi_print("[SYSMAN] UIManager started as PID ");
    maahi_putchar('0' + uimanager_pid);
    maahi_print("\n");
    
    __asm__ volatile(
        "pushl %%eax\n"
        "pushl %%edx\n"
        "movl $0x3F8, %%edx\n"
        "movb $'C', %%al\n" "outb %%al, %%dx\n"
        "movb $'3', %%al\n" "outb %%al, %%dx\n"
        "movb $'\\n', %%al\n" "outb %%al, %%dx\n"
        "popl %%edx\n"
        "popl %%eax\n"
        ::: "memory"
    );
    
    // Clear screen with dark blue background (framebuffer now owned by UIManager)
    maahi_print("[SYSMAN] Clearing screen for Orbit...\n");
    maahi_fill_rect(0, 0, 800, 600, 0x001020);
    
    __asm__ volatile(
        "pushl %%eax\n"
        "pushl %%edx\n"
        "movl $0x3F8, %%edx\n"
        "movb $'C', %%al\n" "outb %%al, %%dx\n"
        "movb $'4', %%al\n" "outb %%al, %%dx\n"
        "movb $'\\n', %%al\n" "outb %%al, %%dx\n"
        "popl %%edx\n"
        "popl %%eax\n"
        ::: "memory"
    );
    
    // Get orbit module address from kernel
    unsigned int orbit_addr = maahi_get_orbit_address();
    
    if (orbit_addr == 0) {
        maahi_draw_text(450, 420, "ERROR: ORBIT NOT LOADED", 0xFF0000, 0);
        while(1) __asm__ volatile("hlt");
    }
    
    maahi_print("[SYSMAN] Orbit address: 0x");
    for (int i = 28; i >= 0; i -= 4) {
        char c = "0123456789ABCDEF"[(orbit_addr >> i) & 0xF];
        maahi_putchar(c);
    }
    maahi_print("\n");
    
    // Create orbit as process 3 (desktop shell)
    maahi_print("[SYSMAN] Creating orbit...\n");
    int orbit_pid = maahi_create_process(orbit_addr);
    
    __asm__ volatile(
        "pushl %%eax\n"
        "pushl %%edx\n"
        "movl $0x3F8, %%edx\n"
        "movb $'C', %%al\n" "outb %%al, %%dx\n"
        "movb $'5', %%al\n" "outb %%al, %%dx\n"
        "movb $'\\n', %%al\n" "outb %%al, %%dx\n"
        "popl %%edx\n"
        "popl %%eax\n"
        ::: "memory"
    );
    
    if (orbit_pid < 0) {
        maahi_draw_text(450, 420, "ERROR: FAILED TO START ORBIT", 0xFF0000, 0);
        while(1) __asm__ volatile("hlt");
    }
    
    maahi_print("[SYSMAN] Orbit started as PID ");
    maahi_putchar('0' + orbit_pid);
    maahi_print("\n");
    
    // Sysman's job is done - just idle forever
    // Timer will switch between processes automatically
    maahi_print("[SYSMAN] Idle loop, timer handles multitasking\n");
    while(1) {
        // Empty loop - preemptive multitasking via timer
        // No yield needed - timer IRQ switches processes
        __asm__ volatile("nop");
    }
}