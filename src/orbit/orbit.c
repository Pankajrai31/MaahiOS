#include "../libgui/libgui.h"
#include "../libgui/cursor_compositor.h"
#include "../syscalls/user_syscalls.h"
#include "../../libraries/icons/embedded_icons.h"

/**
 * Orbit - MaahiOS Desktop Shell
 * Simple button-based interface with mouse support
 */

// Helper function to convert integer to string
static void int_to_str(int num, char *buf) {
    int i = 0, j = 0;
    char temp[16];
    
    if (num == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    
    if (num < 0) {
        buf[j++] = '-';
        num = -num;
    }
    
    while (num > 0) {
        temp[i++] = '0' + (num % 10);
        num /= 10;
    }
    
    while (i > 0) {
        buf[j++] = temp[--i];
    }
    buf[j] = '\0';
}

void orbit_main_c(void) {
    // Initialize cursor compositor
    orbit_cursor_init();
    
    // Simplest possible test
    syscall_puts("[ORBIT_ENTRY] orbit_main_c started!\n");
    
    // FIRST THING: Check PIC masks in Ring 3 IMMEDIATELY
    unsigned int pic_mask = syscall_get_pic_mask();
    unsigned char master = pic_mask & 0xFF;
    unsigned char slave = (pic_mask >> 8) & 0xFF;
    
    syscall_puts("[ORBIT_START] master=0x");
    char hex_str[3];
    const char hex_chars[] = "0123456789ABCDEF";
    hex_str[0] = hex_chars[(master >> 4) & 0xF];
    hex_str[1] = hex_chars[master & 0xF];
    hex_str[2] = '\0';
    syscall_puts(hex_str);
    syscall_puts(" slave=0x");
    hex_str[0] = hex_chars[(slave >> 4) & 0xF];
    hex_str[1] = hex_chars[slave & 0xF];
    hex_str[2] = '\0';
    syscall_puts(hex_str);
    
    // Check if IRQ2 (cascade) is masked
    if (master & (1 << 2)) {
        syscall_puts(" IRQ2_MASKED!");
    } else {
        syscall_puts(" IRQ2_OK");
    }
    
    // Check if IRQ12 is masked
    if (slave & (1 << 4)) {
        syscall_puts(" IRQ12_MASKED!");
    } else {
        syscall_puts(" IRQ12_OK");
    }
    syscall_puts("\n");
    
    // Clear screen to dark blue background
    gui_clear_screen(0x001020);
    
    // Draw buttons
    gui_button("Process Manager", 20, 20);
    gui_button("Disk Manager", 20, 90);
    gui_button("File Explorer", 20, 160);
    gui_button("Notebook", 20, 230);
    
    gui_draw_text(300, 40, "MaahiOS Desktop - Move your mouse!", 0xFFFF00, 0);
    
    // Test: Draw file icon - check if icon data is valid
    // Read first two bytes to verify BMP signature
    uint8_t byte0 = icon_file_bmp[0];
    uint8_t byte1 = icon_file_bmp[1];
    
    if (byte0 == 0x42 && byte1 == 0x4D) {
        syscall_puts("Icon signature OK in Ring3!\n");
    } else {
        syscall_puts("Icon signature INVALID in Ring3!\n");
    }
    
    syscall_draw_bmp(200, 165, (unsigned int)icon_file_bmp);
    syscall_puts("Icon syscall complete\n");
    
    // Main event loop - clean and silent
    static int last_irq_count = 0;
    static int polls_since_irq = 0;
    
    while(1) {
        // Small delay for responsive polling
        for (volatile int i = 0; i < 1000; i++);
        
        // Get current mouse position
        int x = syscall_mouse_get_x();
        int y = syscall_mouse_get_y();
        int irq = syscall_mouse_get_irq_total();
        
        // WORKAROUND: If IRQ12 stopped firing, manually poll 8042
        // QEMU bug: IRQ12 stops firing randomly even though 8042 has data
        if (irq == last_irq_count) {
            polls_since_irq++;
            if (polls_since_irq > 2) {
                syscall_poll_mouse();
            }
        } else {
            polls_since_irq = 0;
            last_irq_count = irq;
        }
        
        // Update cursor position (with built-in change detection)
        orbit_draw_cursor(x, y);
    }
}
